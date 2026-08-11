/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "embedlitesurface_p.h"

#include <QCoreApplication>
#include <QEnableSharedFromThis>
#include <QEvent>
#include <QMutex>
#include <QThread>

#include "mozilla/embedlite/EmbedLiteApp.h"
#include "mozilla/embedlite/EmbedLiteWindow.h"

using namespace mozilla::embedlite;

namespace QtMoz {
namespace {

class EmbedLiteSurface;

class DestroyEvent final : public QEvent
{
public:
    DestroyEvent(const QSharedPointer<EmbedLiteSurface> &surface,
                 EmbedLiteWindow *window);

    void dispatch();

    static QEvent::Type eventType()
    {
        static const QEvent::Type type = static_cast<QEvent::Type>(
                QEvent::registerEventType());
        return type;
    }

private:
    QSharedPointer<EmbedLiteSurface> mSurface;
    EmbedLiteWindow *mWindow;
};

class CallbackDispatcher final : public QObject
{
protected:
    bool event(QEvent *event) override
    {
        if (event->type() == DestroyEvent::eventType()) {
            static_cast<DestroyEvent *>(event)->dispatch();
            return true;
        }
        return QObject::event(event);
    }
};

CallbackDispatcher *callbackDispatcher()
{
    // The process-wide dispatcher must outlive every queued surface teardown.
    static CallbackDispatcher * const dispatcher = new CallbackDispatcher;
    return dispatcher;
}

class MutexLocker final
{
public:
    explicit MutexLocker(QMutex *mutex)
        : mMutex(mutex)
    {
        mMutex->lock();
    }

    ~MutexLocker()
    {
        mMutex->unlock();
    }

private:
    QMutex *mMutex;

    Q_DISABLE_COPY(MutexLocker)
};

class Q_DECL_HIDDEN EmbedLiteSurface final
    : public QMozSurface
    , public QEnableSharedFromThis<EmbedLiteSurface>
{
public:
    EmbedLiteSurface(EmbedLiteApp *app, EmbedLiteWindowListener *listener)
        : mApp(app)
        , mListener(listener)
        , mWindow(nullptr)
        , mOwnerThread(QThread::currentThread())
        , mDispatcher(callbackDispatcher())
        , mActiveCalls(0)
        , mDestroyRequested(false)
        , mDestroyIssued(false)
        , mDestroyed(false)
    {
        Q_ASSERT(mApp);
        Q_ASSERT(mListener);
        Q_ASSERT(mDispatcher->thread() == mOwnerThread);
    }

    ~EmbedLiteSurface() override
    {
        MutexLocker lock(&mMutex);
        Q_ASSERT(!mWindow);
        Q_ASSERT(mActiveCalls == 0);
    }

    EmbedLiteWindow *reserve(const QSize &size)
    {
        {
            MutexLocker lock(&mMutex);
            Q_ASSERT(!mWindow);
            Q_ASSERT(!mDestroyRequested);
            Q_ASSERT(!mDestroyed);
        }

        EmbedLiteWindow * const window = mApp->CreateWindow(
                size.width(), size.height(), mListener);

        bool destroyWindow = false;
        {
            MutexLocker lock(&mMutex);
            if (mDestroyed) {
                return nullptr;
            }
            if (mDestroyRequested) {
                mWindow = window;
                if (!mDestroyIssued && window) {
                    mDestroyIssued = true;
                    destroyWindow = true;
                }
            } else {
                mWindow = window;
            }
        }

        if (destroyWindow) {
            dispatchDestroy(window);
        }
        return window;
    }

    void requestDestroy() override
    {
        EmbedLiteWindow *window = nullptr;
        {
            MutexLocker lock(&mMutex);
            if (mDestroyRequested || mDestroyed) {
                return;
            }
            mDestroyRequested = true;
            if (mActiveCalls == 0 && mWindow) {
                mDestroyIssued = true;
                window = mWindow;
            }
        }
        if (window) {
            dispatchDestroy(window);
        }
    }

    void backendDestroyed() override
    {
        MutexLocker lock(&mMutex);
        mWindow = nullptr;
        mDestroyRequested = true;
        mDestroyIssued = true;
        mDestroyed = true;
    }

    bool setSize(const QSize &size) override
    {
        NativeCall call(this);
        if (!call) {
            return false;
        }
        call.window()->SetSize(size.width(), size.height());
        return true;
    }

    bool setContentOrientation(QMozSurfaceRotation rotation) override
    {
        NativeCall call(this);
        if (!call) {
            return false;
        }

        ScreenRotation embedLiteRotation;
        switch (rotation) {
        case QMozSurfaceRotation::Rotation0:
            embedLiteRotation = ROTATION_0;
            break;
        case QMozSurfaceRotation::Rotation90:
            embedLiteRotation = ROTATION_90;
            break;
        case QMozSurfaceRotation::Rotation180:
            embedLiteRotation = ROTATION_180;
            break;
        case QMozSurfaceRotation::Rotation270:
            embedLiteRotation = ROTATION_270;
            break;
        }

        call.window()->SetContentOrientation(embedLiteRotation);
        return true;
    }

    bool withPlatformImage(
            const QMozSurfaceImageCallback &callback) override
    {
        if (!callback) {
            return false;
        }

        NativeCall call(this);
        if (!call) {
            return false;
        }

        bool accepted = false;
        const bool delivered = call.window()->WithPlatformImage(
                [&](const PlatformImageDescriptor &descriptor) {
            if (descriptor.handleType != PlatformImageHandleType::EGLImage
                    || !descriptor.handle
                    || descriptor.width <= 0
                    || descriptor.height <= 0) {
                return;
            }

            QMozSurfaceTextureTarget textureTarget;
            switch (descriptor.textureTarget) {
            case PlatformImageTextureTarget::Texture2D:
                textureTarget = QMozSurfaceTextureTarget::Texture2D;
                break;
            case PlatformImageTextureTarget::ExternalOES:
                textureTarget = QMozSurfaceTextureTarget::ExternalOES;
                break;
            default:
                return;
            }

            accepted = true;
            callback({
                descriptor.handle,
                QSize(descriptor.width, descriptor.height),
                textureTarget
            });
        });
        return delivered && accepted;
    }

    bool clearPlatformImage() override
    {
        NativeCall call(this);
        if (!call) {
            return false;
        }
        call.window()->ClearPlatformImage();
        return true;
    }

    bool suspendRendering() override
    {
        NativeCall call(this);
        if (!call) {
            return false;
        }
        call.window()->SuspendRendering();
        return true;
    }

    bool resumeRendering() override
    {
        NativeCall call(this);
        if (!call) {
            return false;
        }
        call.window()->ResumeRendering();
        return true;
    }

    bool scheduleUpdate() override
    {
        NativeCall call(this);
        if (!call) {
            return false;
        }
        call.window()->ScheduleUpdate();
        return true;
    }

    bool withWindow(const EmbedLiteWindowCallback &callback)
    {
        if (!callback) {
            return false;
        }

        NativeCall call(this);
        if (!call) {
            return false;
        }
        callback(call.window());
        return true;
    }

private:
    // A native call pins the window without holding mMutex across Gecko or
    // user code. Teardown blocks new calls and is posted to the owner thread
    // after the final pin is released.
    class NativeCall final
    {
    public:
        explicit NativeCall(EmbedLiteSurface *surface)
            : mSurface(surface)
            , mWindow(surface->beginCall())
        {
        }

        ~NativeCall()
        {
            if (mWindow) {
                mSurface->endCall();
            }
        }

        explicit operator bool() const
        {
            return mWindow != nullptr;
        }

        EmbedLiteWindow *window() const
        {
            return mWindow;
        }

    private:
        EmbedLiteSurface *mSurface;
        EmbedLiteWindow *mWindow;

        Q_DISABLE_COPY(NativeCall)
    };

    EmbedLiteWindow *beginCall()
    {
        MutexLocker lock(&mMutex);
        if (!mWindow || mDestroyRequested || mDestroyed) {
            return nullptr;
        }
        ++mActiveCalls;
        return mWindow;
    }

    void endCall()
    {
        EmbedLiteWindow *window = nullptr;
        {
            MutexLocker lock(&mMutex);
            Q_ASSERT(mActiveCalls > 0);
            --mActiveCalls;
            if (mActiveCalls == 0 && mDestroyRequested
                    && !mDestroyIssued && mWindow) {
                mDestroyIssued = true;
                window = mWindow;
            }
        }
        if (window) {
            dispatchDestroy(window);
        }
    }

    void dispatchDestroy(EmbedLiteWindow *window)
    {
        if (QThread::currentThread() == mOwnerThread) {
            destroyOnOwnerThread(window);
            return;
        }

        const QSharedPointer<EmbedLiteSurface> self = sharedFromThis();
        Q_ASSERT(!self.isNull());
        if (self.isNull()) {
            return;
        }
        QCoreApplication::postEvent(mDispatcher,
                                    new DestroyEvent(self, window));
    }

    void destroyOnOwnerThread(EmbedLiteWindow *window)
    {
        Q_ASSERT(QThread::currentThread() == mOwnerThread);
        {
            MutexLocker lock(&mMutex);
            if (mDestroyed || mWindow != window) {
                return;
            }
        }
        mApp->DestroyWindow(window);
    }

    EmbedLiteApp *mApp;
    EmbedLiteWindowListener *mListener;
    EmbedLiteWindow *mWindow;
    QThread *mOwnerThread;
    CallbackDispatcher *mDispatcher;
    QMutex mMutex;
    int mActiveCalls;
    bool mDestroyRequested;
    bool mDestroyIssued;
    bool mDestroyed;

    friend class DestroyEvent;

    Q_DISABLE_COPY(EmbedLiteSurface)
};

DestroyEvent::DestroyEvent(
        const QSharedPointer<EmbedLiteSurface> &surface,
        EmbedLiteWindow *window)
    : QEvent(eventType())
    , mSurface(surface)
    , mWindow(window)
{
}

void DestroyEvent::dispatch()
{
    mSurface->destroyOnOwnerThread(mWindow);
}

EmbedLiteSurface *embedLiteSurface(
        const QSharedPointer<QMozSurface> &surface)
{
    return dynamic_cast<EmbedLiteSurface *>(surface.data());
}

} // namespace

QSharedPointer<QMozSurface> createEmbedLiteSurface(
        EmbedLiteApp *app, EmbedLiteWindowListener *listener)
{
    const QSharedPointer<EmbedLiteSurface> surface(
            new EmbedLiteSurface(app, listener));
    return surface;
}

EmbedLiteWindow *reserveEmbedLiteSurface(
        const QSharedPointer<QMozSurface> &surface, const QSize &size)
{
    EmbedLiteSurface * const embedSurface = embedLiteSurface(surface);
    return embedSurface ? embedSurface->reserve(size) : nullptr;
}

bool withEmbedLiteWindow(const QSharedPointer<QMozSurface> &surface,
                         const EmbedLiteWindowCallback &callback)
{
    EmbedLiteSurface * const embedSurface = embedLiteSurface(surface);
    return embedSurface && embedSurface->withWindow(callback);
}

} // namespace QtMoz

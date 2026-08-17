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

class SurfaceEvent final : public QEvent
{
public:
    enum Kind {
        ContinueDestroy,
        FrameReady,
        FrameDeliveryStopped,
        ChromeWindowInitializationFailed
    };

    SurfaceEvent(const QSharedPointer<EmbedLiteSurface> &surface, Kind kind,
                 quint64 generation = 0,
                 const QMozSurfaceFrameToken &token = { 0, 0 });

    void dispatch();

    static QEvent::Type eventType()
    {
        static const QEvent::Type type = static_cast<QEvent::Type>(
                QEvent::registerEventType());
        return type;
    }

private:
    QSharedPointer<EmbedLiteSurface> mSurface;
    Kind mKind;
    quint64 mGeneration;
    QMozSurfaceFrameToken mToken;
};

class CallbackDispatcher final : public QObject
{
protected:
    bool event(QEvent *event) override
    {
        if (event->type() == DestroyEvent::eventType()) {
            static_cast<DestroyEvent *>(event)->dispatch();
            return true;
        } else if (event->type() == SurfaceEvent::eventType()) {
            static_cast<SurfaceEvent *>(event)->dispatch();
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
    , public EmbedLiteWindowListener
    , public EmbedLiteChromeWindowListener
    , public EmbedLitePlatformFrameListener
    , public QEnableSharedFromThis<EmbedLiteSurface>
{
public:
    EmbedLiteSurface(
            EmbedLiteApp *app, EmbedLiteWindowListener *listener,
            const EmbedLiteChromeWindowFailedCallback &chromeWindowFailed)
        : mApp(app)
        , mListener(listener)
        , mChromeWindowFailed(chromeWindowFailed)
        , mWindow(nullptr)
        , mOwnerThread(QThread::currentThread())
        , mDispatcher(callbackDispatcher())
        , mActiveCalls(0)
        , mDestroyRequested(false)
        , mDestroyIssued(false)
        , mDestroyed(false)
        , mFrameDeliveryEnabled(false)
        , mFrameDeliveryStopPending(false)
        , mFrameEventGeneration(0)
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

    EmbedLiteWindow *reserve(const QSize &size, bool chromeHosted,
                             const QByteArray &chromeInitialUrl)
    {
        {
            MutexLocker lock(&mMutex);
            Q_ASSERT(!mWindow);
            Q_ASSERT(!mDestroyRequested);
            Q_ASSERT(!mDestroyed);
        }

        EmbedLiteWindow * const window = !chromeHosted
                ? mApp->CreateWindow(size.width(), size.height(), this)
                : chromeInitialUrl.isEmpty()
                  ? mApp->CreateChromeTabWindow(
                    size.width(), size.height(), this)
                  : mApp->CreateChromeWindow(
                    size.width(), size.height(),
                    chromeInitialUrl.constData(), this);
        if (window && !window->SetPlatformFrameListener(this)) {
            mApp->DestroyWindow(window);
            return nullptr;
        }

        bool continueDestroy = false;
        {
            MutexLocker lock(&mMutex);
            if (mDestroyed) {
                return nullptr;
            }
            if (mDestroyRequested) {
                mWindow = window;
                continueDestroy = window != nullptr;
            } else {
                mWindow = window;
            }
        }

        if (continueDestroy) {
            scheduleDestroyContinuation();
        }
        return window;
    }

    void requestDestroy() override
    {
        {
            MutexLocker lock(&mMutex);
            if (mDestroyRequested || mDestroyed) {
                return;
            }
            mDestroyRequested = true;
        }
        if (QThread::currentThread() == mOwnerThread) {
            continueDestroyOnOwnerThread();
        } else {
            scheduleDestroyContinuation();
        }
    }

    void backendDestroyed() override
    {
        MutexLocker lock(&mMutex);
        mWindow = nullptr;
        mDestroyRequested = true;
        mDestroyIssued = true;
        mDestroyed = true;
        mFrameDeliveryEnabled = false;
        mFrameDeliveryStopPending = false;
        mFrameReadyCallback = QMozSurfaceFrameReadyCallback();
        mFrameDeliveryStoppedCallback =
                QMozSurfaceFrameDeliveryStoppedCallback();
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

    bool setPlatformFrameCallbacks(
            const QMozSurfaceFrameReadyCallback &readyCallback,
            const QMozSurfaceFrameDeliveryStoppedCallback &stoppedCallback)
            override
    {
        MutexLocker lock(&mMutex);
        if (mDestroyRequested || mDestroyed || mFrameDeliveryEnabled
                || mFrameDeliveryStopPending) {
            return false;
        }
        mFrameReadyCallback = readyCallback;
        mFrameDeliveryStoppedCallback = stoppedCallback;
        return true;
    }

    bool setPlatformFrameDeliveryEnabled(bool enabled) override
    {
        MutexLocker controlLock(&mFrameControlMutex);
        quint64 generation = 0;
        {
            MutexLocker lock(&mMutex);
            if (mDestroyRequested || mDestroyed || mDestroyIssued
                    || mFrameDeliveryStopPending) {
                return false;
            }
            if (mFrameDeliveryEnabled == enabled) {
                return true;
            }
            if (enabled && !mFrameReadyCallback) {
                return false;
            }
            mFrameDeliveryEnabled = enabled;
            mFrameDeliveryStopPending = !enabled;
            if (enabled) {
                ++mFrameEventGeneration;
                if (!mFrameEventGeneration) {
                    ++mFrameEventGeneration;
                }
            }
            generation = mFrameEventGeneration;
        }

        NativeCall call(this);
        if (!call
                || !call.window()->SetPlatformFrameDeliveryEnabled(enabled)) {
            MutexLocker lock(&mMutex);
            if (generation == mFrameEventGeneration) {
                mFrameDeliveryEnabled = !enabled;
                mFrameDeliveryStopPending = false;
            }
            return false;
        }
        return true;
    }

    bool acquirePlatformFrame(
            const QMozSurfaceFrameToken &token,
            const QMozSurfaceFrameCallback &callback) override
    {
        if (!token.isValid() || !callback) {
            return false;
        }

        NativeCall call(this);
        if (!call) {
            return false;
        }

        bool accepted = false;
        const PlatformFrameToken embedToken = { token.epoch, token.sequence };
        const bool delivered = call.window()->AcquirePlatformFrame(
                embedToken,
                [&](const PlatformFrameDescriptor &descriptor) {
            QMozSurfaceFrame frame;
            if (!convertPlatformFrame(descriptor, &frame)
                    || frame.token.epoch != token.epoch
                    || frame.token.sequence != token.sequence) {
                return false;
            }
            accepted = callback(frame);
            return accepted;
        });
        return delivered && accepted;
    }

    bool releasePlatformFrame(
            const QMozSurfaceFrameRelease &release) override
    {
        if (!release.token.isValid()) {
            return false;
        }

        NativeCall call(this, NativeCall::AllowDuringDestroy);
        if (!call) {
            return false;
        }

        PlatformFrameFenceHandleType fenceType;
        switch (release.fenceType) {
        case QMozSurfaceFrameFenceType::NoHandle:
            if (release.fence) {
                return false;
            }
            fenceType = PlatformFrameFenceHandleType::NoHandle;
            break;
        case QMozSurfaceFrameFenceType::EGLSync:
            if (!release.fence) {
                return false;
            }
            fenceType = PlatformFrameFenceHandleType::EGLSync;
            break;
        default:
            return false;
        }

        return call.window()->ReleasePlatformFrame({
            { release.token.epoch, release.token.sequence },
            fenceType,
            release.fence
        });
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

    void WindowInitialized() override
    {
        mListener->WindowInitialized();
    }

    void WindowDestroyed() override
    {
        // The client removes the registry's owning reference from this
        // callback. Keep the forwarding listener alive until it unwinds.
        const QSharedPointer<EmbedLiteSurface> self = sharedFromThis();
        Q_ASSERT(!self.isNull());
        mListener->WindowDestroyed();
    }

    void DrawOverlay(const nsIntRect &rect) override
    {
        mListener->DrawOverlay(rect);
    }

    bool PreRender() override
    {
        return mListener->PreRender();
    }

    void CompositorCreated() override
    {
        mListener->CompositorCreated();
    }

    void CompositingFinished() override
    {
        mListener->CompositingFinished();
    }

    void ChromeWindowInitializationFailed() override
    {
        if (QThread::currentThread() == mOwnerThread) {
            dispatchSurfaceEvent(
                    SurfaceEvent::ChromeWindowInitializationFailed, 0,
                    { 0, 0 });
        } else {
            postSurfaceEvent(
                    SurfaceEvent::ChromeWindowInitializationFailed, 0);
        }
    }

    void PlatformFrameReady(const PlatformFrameToken &token) override
    {
        MutexLocker controlLock(&mFrameControlMutex);
        quint64 generation = 0;
        {
            MutexLocker lock(&mMutex);
            if (mDestroyRequested || mDestroyed || !mFrameDeliveryEnabled
                    || mFrameDeliveryStopPending) {
                return;
            }
            generation = mFrameEventGeneration;
        }
        postSurfaceEvent(SurfaceEvent::FrameReady, generation,
                         { token.epoch, token.sequence });
    }

    void PlatformFrameDeliveryStopped() override
    {
        MutexLocker controlLock(&mFrameControlMutex);
        quint64 generation = 0;
        {
            MutexLocker lock(&mMutex);
            if (mDestroyed || !mFrameDeliveryStopPending) {
                return;
            }
            generation = mFrameEventGeneration;
        }
        postSurfaceEvent(SurfaceEvent::FrameDeliveryStopped, generation);
    }

private:
    static bool convertPlatformFrame(
            const PlatformFrameDescriptor &descriptor,
            QMozSurfaceFrame *frame)
    {
        if (!frame || !descriptor.token.IsValid()
                || descriptor.image.handleType !=
                        PlatformImageHandleType::EGLImage
                || !descriptor.image.handle
                || descriptor.image.width <= 0
                || descriptor.image.height <= 0) {
            return false;
        }

        switch (descriptor.image.textureTarget) {
        case PlatformImageTextureTarget::Texture2D:
            frame->image.textureTarget =
                    QMozSurfaceTextureTarget::Texture2D;
            break;
        case PlatformImageTextureTarget::ExternalOES:
            frame->image.textureTarget =
                    QMozSurfaceTextureTarget::ExternalOES;
            break;
        default:
            return false;
        }

        switch (descriptor.releaseFenceHandleType) {
        case PlatformFrameFenceHandleType::NoHandle:
            frame->releaseFenceType =
                    QMozSurfaceFrameFenceType::NoHandle;
            break;
        case PlatformFrameFenceHandleType::EGLSync:
            frame->releaseFenceType =
                    QMozSurfaceFrameFenceType::EGLSync;
            break;
        default:
            return false;
        }

        frame->token = { descriptor.token.epoch,
                         descriptor.token.sequence };
        frame->image.handle = descriptor.image.handle;
        frame->image.size = QSize(descriptor.image.width,
                                  descriptor.image.height);
        return true;
    }

    // A native call pins the window without holding mMutex across Gecko or
    // user code. Teardown blocks new calls except frame release, and is
    // continued on the owner thread after the final pin is released.
    class NativeCall final
    {
    public:
        enum Policy {
            RejectDuringDestroy,
            AllowDuringDestroy
        };

        explicit NativeCall(EmbedLiteSurface *surface,
                            Policy policy = RejectDuringDestroy)
            : mSurface(surface)
            , mWindow(surface->beginCall(policy))
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

    EmbedLiteWindow *beginCall(NativeCall::Policy policy)
    {
        MutexLocker lock(&mMutex);
        if (!mWindow || mDestroyIssued || mDestroyed
                || (mDestroyRequested
                    && policy == NativeCall::RejectDuringDestroy)) {
            return nullptr;
        }
        ++mActiveCalls;
        return mWindow;
    }

    void endCall()
    {
        bool continueDestroy = false;
        {
            MutexLocker lock(&mMutex);
            Q_ASSERT(mActiveCalls > 0);
            --mActiveCalls;
            continueDestroy = mActiveCalls == 0 && mDestroyRequested
                    && !mDestroyIssued && mWindow;
        }
        if (continueDestroy) {
            scheduleDestroyContinuation();
        }
    }

    void scheduleDestroyContinuation()
    {
        postSurfaceEvent(SurfaceEvent::ContinueDestroy, 0);
    }

    void continueDestroyOnOwnerThread()
    {
        Q_ASSERT(QThread::currentThread() == mOwnerThread);
        MutexLocker controlLock(&mFrameControlMutex);

        EmbedLiteWindow *window = nullptr;
        bool disableDelivery = false;
        {
            MutexLocker lock(&mMutex);
            if (!mDestroyRequested || mDestroyIssued || mDestroyed
                    || !mWindow || mActiveCalls != 0
                    || mFrameDeliveryStopPending) {
                return;
            }
            window = mWindow;
            if (mFrameDeliveryEnabled) {
                ++mActiveCalls;
                disableDelivery = true;
                mFrameDeliveryEnabled = false;
                mFrameDeliveryStopPending = true;
            } else {
                mDestroyIssued = true;
            }
        }

        if (disableDelivery) {
            const bool disabled =
                    window->SetPlatformFrameDeliveryEnabled(false);
            {
                MutexLocker lock(&mMutex);
                Q_ASSERT(mActiveCalls > 0);
                --mActiveCalls;
                if (!disabled) {
                    mFrameDeliveryEnabled = true;
                    mFrameDeliveryStopPending = false;
                }
            }
            // Destruction deliberately remains pending on failure. Retrying
            // or timing out cannot safely retire render-thread GL resources.
            return;
        }

        if (!window->SetPlatformFrameListener(nullptr)) {
            // Fail closed rather than destroy a listener still owned by
            // Gecko's frame-delivery path.
            MutexLocker lock(&mMutex);
            mDestroyIssued = false;
            return;
        }
        dispatchDestroy(window);
    }

    void dispatchSurfaceEvent(SurfaceEvent::Kind kind, quint64 generation,
                              const QMozSurfaceFrameToken &token)
    {
        Q_ASSERT(QThread::currentThread() == mOwnerThread);
        if (kind == SurfaceEvent::ContinueDestroy) {
            continueDestroyOnOwnerThread();
            return;
        }
        if (kind == SurfaceEvent::ChromeWindowInitializationFailed) {
            EmbedLiteChromeWindowFailedCallback callback;
            {
                MutexLocker lock(&mMutex);
                callback = mChromeWindowFailed;
            }
            if (callback) {
                callback();
            }
            return;
        }

        QMozSurfaceFrameReadyCallback readyCallback;
        QMozSurfaceFrameDeliveryStoppedCallback stoppedCallback;
        bool continueDestroy = false;
        {
            MutexLocker controlLock(&mFrameControlMutex);
            MutexLocker lock(&mMutex);
            if (mDestroyed || generation != mFrameEventGeneration) {
                return;
            }
            if (kind == SurfaceEvent::FrameReady) {
                if (mDestroyRequested || !mFrameDeliveryEnabled
                        || mFrameDeliveryStopPending) {
                    return;
                }
                readyCallback = mFrameReadyCallback;
            } else if (kind == SurfaceEvent::FrameDeliveryStopped) {
                if (!mFrameDeliveryStopPending) {
                    return;
                }
                mFrameDeliveryStopPending = false;
                stoppedCallback = mFrameDeliveryStoppedCallback;
                continueDestroy = mDestroyRequested;
            }
        }

        if (readyCallback) {
            readyCallback(token);
        } else if (stoppedCallback) {
            stoppedCallback();
        }
        if (continueDestroy) {
            continueDestroyOnOwnerThread();
        }
    }

    void postSurfaceEvent(
            SurfaceEvent::Kind kind, quint64 generation,
            const QMozSurfaceFrameToken &token = { 0, 0 })
    {
        const QSharedPointer<EmbedLiteSurface> self = sharedFromThis();
        Q_ASSERT(!self.isNull());
        if (self.isNull()) {
            return;
        }
        QCoreApplication::postEvent(
                mDispatcher,
                new SurfaceEvent(self, kind, generation, token));
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
    EmbedLiteChromeWindowFailedCallback mChromeWindowFailed;
    EmbedLiteWindow *mWindow;
    QThread *mOwnerThread;
    CallbackDispatcher *mDispatcher;
    QMutex mMutex;
    QMutex mFrameControlMutex;
    int mActiveCalls;
    bool mDestroyRequested;
    bool mDestroyIssued;
    bool mDestroyed;
    bool mFrameDeliveryEnabled;
    bool mFrameDeliveryStopPending;
    quint64 mFrameEventGeneration;
    QMozSurfaceFrameReadyCallback mFrameReadyCallback;
    QMozSurfaceFrameDeliveryStoppedCallback
            mFrameDeliveryStoppedCallback;

    friend class DestroyEvent;
    friend class SurfaceEvent;

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

SurfaceEvent::SurfaceEvent(
        const QSharedPointer<EmbedLiteSurface> &surface, Kind kind,
        quint64 generation, const QMozSurfaceFrameToken &token)
    : QEvent(eventType())
    , mSurface(surface)
    , mKind(kind)
    , mGeneration(generation)
    , mToken(token)
{
}

void SurfaceEvent::dispatch()
{
    mSurface->dispatchSurfaceEvent(mKind, mGeneration, mToken);
}

EmbedLiteSurface *embedLiteSurface(
        const QSharedPointer<QMozSurface> &surface)
{
    return dynamic_cast<EmbedLiteSurface *>(surface.data());
}

} // namespace

QSharedPointer<QMozSurface> createEmbedLiteSurface(
        EmbedLiteApp *app, EmbedLiteWindowListener *listener,
        const EmbedLiteChromeWindowFailedCallback &chromeWindowFailed)
{
    const QSharedPointer<EmbedLiteSurface> surface(
            new EmbedLiteSurface(app, listener, chromeWindowFailed));
    return surface;
}

EmbedLiteWindow *reserveEmbedLiteSurface(
        const QSharedPointer<QMozSurface> &surface, const QSize &size,
        bool chromeHosted, const QByteArray &chromeInitialUrl)
{
    EmbedLiteSurface * const embedSurface = embedLiteSurface(surface);
    return embedSurface
            ? embedSurface->reserve(size, chromeHosted, chromeInitialUrl)
            : nullptr;
}

bool withEmbedLiteWindow(const QSharedPointer<QMozSurface> &surface,
                         const EmbedLiteWindowCallback &callback)
{
    EmbedLiteSurface * const embedSurface = embedLiteSurface(surface);
    return embedSurface && embedSurface->withWindow(callback);
}

} // namespace QtMoz

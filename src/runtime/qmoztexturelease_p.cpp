/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "qmoztexturelease_p.h"

#include "qmozframestream_p.h"
#include "../qmozembedlog.h"
#include "../qmozexttexture.h"

#include <QHash>
#include <QCoreApplication>
#include <QMutex>
#include <QMutexLocker>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QPointer>
#include <QQuickWindow>
#include <QRunnable>
#include <QSet>
#include <QSharedPointer>
#include <QVector>

#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace QtMoz {
namespace {

struct HeldFrame
{
    QMozSurfaceFrameToken token = { 0, 0 };
    QMozSurfaceFrameFenceType fenceType =
            QMozSurfaceFrameFenceType::NoHandle;
    QPointer<QOpenGLContext> context;

    bool isValid() const
    {
        return token.isValid();
    }
};

class TextureFrameLease final
{
public:
    TextureFrameLease(const QSharedPointer<QMozFrameStream> &stream,
                      const void *consumer, QQuickWindow *renderWindow,
                      const QMozTextureFrameInvalidated &invalidatedCallback)
        : mStream(stream)
        , mConsumer(consumer)
        , mRenderWindow(renderWindow)
        , mRenderWindowIdentity(renderWindow)
        , mInvalidatedCallback(invalidatedCallback)
    {
        Q_ASSERT(!mStream.isNull());
        Q_ASSERT(mConsumer);
        Q_ASSERT(mRenderWindow);
    }

    bool belongsToWindow(QQuickWindow *window) const
    {
        QMutexLocker lock(&mWindowMutex);
        return window && mRenderWindow.data() == window
                && mRenderWindowIdentity == window;
    }

    QQuickWindow *renderWindow() const
    {
        QMutexLocker lock(&mWindowMutex);
        return mRenderWindow.data();
    }

    bool orphanWindow(QQuickWindow *window)
    {
        QMutexLocker lock(&mWindowMutex);
        if (!window || mRenderWindowIdentity != window) {
            return false;
        }
        mRenderWindow.clear();
        mRenderWindowIdentity = nullptr;
        return true;
    }

    QMozTextureFrameInvalidated invalidatedCallback() const
    {
        return mInvalidatedCallback;
    }

    bool usesPlatformFrames() const
    {
        return mStream->enabled();
    }

    bool acquire(const QMozTextureFrameImport &importCallback,
                 const QMozTextureFrameDiscard &discardCallback)
    {
        QOpenGLContext * const context = QOpenGLContext::currentContext();
        if (!context || !importCallback || !discardCallback
                || !releaseDeferredFrames()) {
            return false;
        }

        QMozSurfaceFrameToken token = { 0, 0 };
        if (!mStream->takePendingFrame(mConsumer, &token)) {
            return false;
        }

        const QSharedPointer<QMozSurface> surface = mStream->surface();
        if (!surface) {
            mStream->restorePendingFrame(mConsumer, token);
            return false;
        }

        QMozSurfaceFrame acquiredFrame = {
            { 0, 0 },
            { nullptr, QSize(), QMozSurfaceTextureTarget::Texture2D },
            QMozSurfaceFrameFenceType::NoHandle
        };
        const bool acquired = surface->acquirePlatformFrame(
                token, [&](const QMozSurfaceFrame &frame) {
            if (frame.token.epoch != token.epoch
                    || frame.token.sequence != token.sequence
                    || !frame.image.handle || frame.image.size.isEmpty()) {
                return false;
            }
            if (!importCallback(frame)) {
                return false;
            }
            acquiredFrame = frame;
            return true;
        });
        if (!acquired) {
            // Drop any texture object created while the image was borrowed
            // before Gecko can retire that image record.
            discardCallback();
            mStream->restorePendingFrame(mConsumer, token);
            return false;
        }

        const HeldFrame next = {
            acquiredFrame.token, acquiredFrame.releaseFenceType, context
        };
        if (!releaseFrame(&mCurrentFrame)) {
            // The replacement was imported but has never been sampled. Drop
            // its GL binding before returning its lease to Gecko.
            discardCallback();
            HeldFrame uncommitted = next;
            if (!releaseFrame(&uncommitted)) {
                mDeferredFrames.append(uncommitted);
            }
            return false;
        }

        mCurrentFrame = next;
        return true;
    }

    bool releaseAll()
    {
        bool released = releaseFrame(&mCurrentFrame);
        released = releaseDeferredFrames() && released;
        return released;
    }

private:
    bool releaseDeferredFrames()
    {
        bool released = true;
        QVector<HeldFrame> pending;
        pending.swap(mDeferredFrames);
        for (HeldFrame &frame : pending) {
            if (!releaseFrame(&frame)) {
                mDeferredFrames.append(frame);
                released = false;
            }
        }
        return released;
    }

    bool releaseFrame(HeldFrame *frame)
    {
        if (!frame || !frame->isValid()) {
            return true;
        }

        QOpenGLContext * const context = QOpenGLContext::currentContext();
        QOpenGLFunctions * const functions =
                context ? context->functions() : nullptr;
        const QSharedPointer<QMozSurface> surface = mStream->surface();
        if (!functions || !surface || frame->context.data() != context) {
            return false;
        }

        if (frame->fenceType == QMozSurfaceFrameFenceType::EGLSync) {
            static const PFNEGLCREATESYNCKHRPROC createSync =
                    reinterpret_cast<PFNEGLCREATESYNCKHRPROC>(
                        eglGetProcAddress("eglCreateSyncKHR"));
            static const PFNEGLDESTROYSYNCKHRPROC destroySync =
                    reinterpret_cast<PFNEGLDESTROYSYNCKHRPROC>(
                        eglGetProcAddress("eglDestroySyncKHR"));
            const EGLDisplay display = eglGetCurrentDisplay();
            if (createSync && destroySync && display != EGL_NO_DISPLAY) {
                const EGLSyncKHR sync = createSync(
                        display, EGL_SYNC_FENCE_KHR, nullptr);
                if (sync != EGL_NO_SYNC_KHR) {
                    functions->glFlush();
                    if (surface->releasePlatformFrame({
                            frame->token,
                            QMozSurfaceFrameFenceType::EGLSync,
                            static_cast<void *>(sync)
                    })) {
                        frame->token = { 0, 0 };
                        frame->context.clear();
                        return true;
                    }
                    if (destroySync(display, sync) != EGL_TRUE) {
                        qCWarning(lcEmbedLiteExt)
                                << "Failed to destroy rejected frame fence";
                    }
                }
            }
        }

        // NoHandle means the caller has synchronously completed every GPU
        // read. It is also the safe fallback when EGL fence transfer fails.
        functions->glFinish();
        if (!surface->releasePlatformFrame({
                frame->token,
                QMozSurfaceFrameFenceType::NoHandle,
                nullptr
        })) {
            return false;
        }
        frame->token = { 0, 0 };
        frame->context.clear();
        return true;
    }

    QSharedPointer<QMozFrameStream> mStream;
    const void *mConsumer;
    mutable QMutex mWindowMutex;
    QPointer<QQuickWindow> mRenderWindow;
    QQuickWindow *mRenderWindowIdentity;
    QMozTextureFrameInvalidated mInvalidatedCallback;
    HeldFrame mCurrentFrame;
    QVector<HeldFrame> mDeferredFrames;

    Q_DISABLE_COPY(TextureFrameLease)
};

class PendingTextureCleanup final
{
public:
    explicit PendingTextureCleanup(QMozExtTexture *texture)
        : mTexture(texture)
        , mTextureIdentity(texture)
    {
        Q_ASSERT(mTexture);
    }

    QMozExtTexture *take()
    {
        QMutexLocker lock(&mMutex);
        QMozExtTexture * const texture = mTexture;
        mTexture = nullptr;
        return texture;
    }

    QMozExtTexture *identity() const
    {
        return mTextureIdentity;
    }

private:
    QMutex mMutex;
    QMozExtTexture *mTexture;
    QMozExtTexture * const mTextureIdentity;

    Q_DISABLE_COPY(PendingTextureCleanup)
};

void finishPendingTextureCleanup(
        QQuickWindow *window,
        const QSharedPointer<PendingTextureCleanup> &cleanup);

class TextureCleanupJob final : public QRunnable
{
public:
    explicit TextureCleanupJob(
            QQuickWindow *window,
            const QSharedPointer<PendingTextureCleanup> &cleanup)
        : mWindow(window)
        , mCleanup(cleanup)
    {
    }

    void run() override
    {
        delete mCleanup->take();
        finishPendingTextureCleanup(mWindow, mCleanup);
    }

private:
    QQuickWindow * const mWindow;
    const QSharedPointer<PendingTextureCleanup> mCleanup;

    Q_DISABLE_COPY(TextureCleanupJob)
};

struct TextureLeaseRegistry
{
    QMutex mutex;
    QHash<QMozExtTexture *, QSharedPointer<TextureFrameLease>> leases;
    QHash<QQuickWindow *, QVector<QSharedPointer<PendingTextureCleanup>>>
            pendingCleanups;
    QSet<QQuickWindow *> watchedWindows;
    QSet<QQuickWindow *> invalidatingWindows;
    QVector<QSharedPointer<TextureFrameLease>> retainedLeases;
    QVector<QSharedPointer<PendingTextureCleanup>> orphanedCleanups;
};

// Retained leases deliberately outlive normal static destruction. Dropping a
// GL-backed lease on an arbitrary shutdown thread would violate the producer
// context's thread contract.
TextureLeaseRegistry *textureLeaseRegistry()
{
    static TextureLeaseRegistry * const registry =
            new TextureLeaseRegistry;
    return registry;
}

void finishPendingTextureCleanup(
        QQuickWindow *window,
        const QSharedPointer<PendingTextureCleanup> &cleanup)
{
    TextureLeaseRegistry * const registry = textureLeaseRegistry();
    QMutexLocker lock(&registry->mutex);
    auto found = registry->pendingCleanups.find(window);
    if (found == registry->pendingCleanups.end()) {
        return;
    }
    found.value().removeAll(cleanup);
    if (found.value().isEmpty()) {
        registry->pendingCleanups.erase(found);
    }
}

QSharedPointer<TextureFrameLease> textureFrameLease(QMozExtTexture *texture)
{
    TextureLeaseRegistry * const registry = textureLeaseRegistry();
    if (!registry || !texture) {
        return QSharedPointer<TextureFrameLease>();
    }

    QMutexLocker lock(&registry->mutex);
    return registry->leases.value(texture);
}

void retryRetainedTextureFrameLeases()
{
    if (!QOpenGLContext::currentContext()) {
        return;
    }

    TextureLeaseRegistry * const registry = textureLeaseRegistry();
    QVector<QSharedPointer<TextureFrameLease>> leases;
    {
        QMutexLocker lock(&registry->mutex);
        leases.swap(registry->retainedLeases);
    }

    QVector<QSharedPointer<TextureFrameLease>> retained;
    for (const QSharedPointer<TextureFrameLease> &lease : leases) {
        if (!lease->releaseAll()) {
            retained.append(lease);
        }
    }
    if (!retained.isEmpty()) {
        QMutexLocker lock(&registry->mutex);
        registry->retainedLeases += retained;
    }
}

void cleanupTextureFrameWindow(QQuickWindow *window)
{
    if (!window) {
        return;
    }

    TextureLeaseRegistry * const registry = textureLeaseRegistry();
    QVector<QSharedPointer<PendingTextureCleanup>> pending;
    QVector<QPair<QMozExtTexture *, QMozTextureFrameInvalidated>> active;
    QSet<QMozExtTexture *> pendingTextures;
    {
        QMutexLocker lock(&registry->mutex);
        registry->invalidatingWindows.insert(window);
        pending = registry->pendingCleanups.take(window);
        for (const QSharedPointer<PendingTextureCleanup> &cleanup : pending) {
            pendingTextures.insert(cleanup->identity());
        }
        for (auto it = registry->leases.constBegin();
                it != registry->leases.constEnd(); ++it) {
            if (it.value()->belongsToWindow(window)) {
                active.append(qMakePair(
                        it.key(), it.value()->invalidatedCallback()));
            }
        }
    }

    for (const QSharedPointer<PendingTextureCleanup> &cleanup : pending) {
        QMozExtTexture * const texture = cleanup->take();
        if (texture) {
            delete texture;
        }
    }
    for (const auto &entry : active) {
        if (pendingTextures.contains(entry.first)) {
            continue;
        }
        if (entry.second) {
            entry.second(entry.first);
        }
        delete entry.first;
    }
    retryRetainedTextureFrameLeases();
}

void orphanTextureFrameWindow(QQuickWindow *window)
{
    if (!window) {
        return;
    }

    TextureLeaseRegistry * const registry = textureLeaseRegistry();
    QVector<QPair<QMozExtTexture *, QMozTextureFrameInvalidated>> active;
    {
        QMutexLocker lock(&registry->mutex);
        registry->watchedWindows.remove(window);
        registry->invalidatingWindows.remove(window);
        registry->orphanedCleanups +=
                registry->pendingCleanups.take(window);
        for (auto it = registry->leases.constBegin();
                it != registry->leases.constEnd(); ++it) {
            if (it.value()->orphanWindow(window)) {
                active.append(qMakePair(
                        it.key(), it.value()->invalidatedCallback()));
            }
        }
    }

    for (const auto &entry : active) {
        if (entry.second) {
            entry.second(entry.first);
        }
    }
    if (!active.isEmpty()) {
        qCCritical(lcEmbedLiteExt)
                << "Retaining platform frames after scene graph loss";
    }
}

void initializeTextureFrameWindow(QQuickWindow *window)
{
    TextureLeaseRegistry * const registry = textureLeaseRegistry();
    {
        QMutexLocker lock(&registry->mutex);
        registry->invalidatingWindows.remove(window);
    }
    retryRetainedTextureFrameLeases();
}

void watchTextureFrameWindow(QQuickWindow *window)
{
    TextureLeaseRegistry * const registry = textureLeaseRegistry();
    {
        QMutexLocker lock(&registry->mutex);
        if (registry->watchedWindows.contains(window)) {
            return;
        }
        registry->watchedWindows.insert(window);
    }

    QObject::connect(window, &QQuickWindow::sceneGraphInvalidated,
                     window, [window]() {
        cleanupTextureFrameWindow(window);
    }, Qt::DirectConnection);
    QObject::connect(window, &QQuickWindow::sceneGraphInitialized,
                     window, [window]() {
        initializeTextureFrameWindow(window);
    }, Qt::DirectConnection);
    QObject::connect(window, &QObject::destroyed,
                     QCoreApplication::instance(), [window]() {
        orphanTextureFrameWindow(window);
    }, Qt::DirectConnection);
}

} // namespace

bool attachTextureFrameLease(
        QMozExtTexture *texture, QMozWindow *window, const void *consumer,
        QQuickWindow *renderWindow,
        const QMozTextureFrameInvalidated &invalidatedCallback)
{
    TextureLeaseRegistry * const registry = textureLeaseRegistry();
    const QSharedPointer<QMozFrameStream> stream =
            windowFrameStream(window);
    if (!texture || !consumer || !renderWindow || !stream) {
        return false;
    }

    const QSharedPointer<TextureFrameLease> lease(
            new TextureFrameLease(stream, consumer, renderWindow,
                                  invalidatedCallback));
    {
        QMutexLocker lock(&registry->mutex);
        if (registry->leases.contains(texture)) {
            return false;
        }
        registry->leases.insert(texture, lease);
    }
    watchTextureFrameWindow(renderWindow);
    return true;
}

bool textureUsesPlatformFrames(QMozExtTexture *texture)
{
    const QSharedPointer<TextureFrameLease> lease =
            textureFrameLease(texture);
    return lease && lease->usesPlatformFrames();
}

bool acquireTexturePlatformFrame(
        QMozExtTexture *texture,
        const QMozTextureFrameImport &importCallback,
        const QMozTextureFrameDiscard &discardCallback)
{
    retryRetainedTextureFrameLeases();
    const QSharedPointer<TextureFrameLease> lease =
            textureFrameLease(texture);
    return lease && lease->acquire(importCallback, discardCallback);
}

bool scheduleTextureFrameCleanup(QMozExtTexture *texture)
{
    TextureLeaseRegistry * const registry = textureLeaseRegistry();
    if (!texture) {
        return false;
    }

    const QSharedPointer<PendingTextureCleanup> cleanup(
            new PendingTextureCleanup(texture));
    QQuickWindow *window = nullptr;
    {
        QMutexLocker lock(&registry->mutex);
        const QSharedPointer<TextureFrameLease> lease =
                registry->leases.value(texture);
        if (!lease) {
            return false;
        }
        window = lease->renderWindow();
        if (!window) {
            registry->orphanedCleanups.append(cleanup);
            qCCritical(lcEmbedLiteExt)
                    << "Retaining platform frame without scene graph window";
            return true;
        }
        if (registry->invalidatingWindows.contains(window)) {
            // The invalidation handler claimed every texture for this window
            // before taking its active snapshot. It will perform the delete.
            return true;
        }
        if (registry->leases.value(texture) != lease
                || !lease->belongsToWindow(window)) {
            return false;
        }
        registry->pendingCleanups[window].append(cleanup);
    }
    window->scheduleRenderJob(new TextureCleanupJob(window, cleanup),
                              QQuickWindow::AfterRenderingStage);
    window->update();
    return true;
}

bool releaseTexturePlatformFrames(QMozExtTexture *texture)
{
    TextureLeaseRegistry * const registry = textureLeaseRegistry();
    if (!registry || !texture) {
        return true;
    }

    QSharedPointer<TextureFrameLease> lease;
    {
        QMutexLocker lock(&registry->mutex);
        lease = registry->leases.take(texture);
    }
    if (!lease || lease->releaseAll()) {
        return true;
    }

    // Without a current compatible GL context there is no truthful
    // NoHandle release. Retain the lease and its surface instead of allowing
    // Gecko to recycle an image that the GPU may still sample.
    QMutexLocker lock(&registry->mutex);
    registry->retainedLeases.append(lease);
    qCCritical(lcEmbedLiteExt)
            << "Retaining platform frame after unsafe texture teardown";
    return false;
}

} // namespace QtMoz

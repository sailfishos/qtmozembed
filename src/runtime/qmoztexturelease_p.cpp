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
#include <QThread>
#include <QTimer>
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
                      quint64 leaseId, quint64 consumerId,
                      const void *frameConsumer,
                      QQuickWindow *renderWindow)
        : mStream(stream)
        , mLeaseId(leaseId)
        , mConsumerId(consumerId)
        , mFrameConsumer(frameConsumer)
        , mRenderWindow(renderWindow)
        , mRenderWindowIdentity(renderWindow)
    {
        Q_ASSERT(!mStream.isNull());
        Q_ASSERT(mLeaseId);
        Q_ASSERT(mConsumerId);
        Q_ASSERT(mFrameConsumer);
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

    quint64 leaseId() const
    {
        return mLeaseId;
    }

    quint64 consumerId() const
    {
        return mConsumerId;
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
        if (!mStream->takePendingFrame(mFrameConsumer, &token)) {
            return false;
        }

        const QSharedPointer<QMozSurface> surface = mStream->surface();
        if (!surface) {
            mStream->restorePendingFrame(mFrameConsumer, token);
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
            mStream->restorePendingFrame(mFrameConsumer, token);
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
    const quint64 mLeaseId;
    const quint64 mConsumerId;
    const void * const mFrameConsumer;
    mutable QMutex mWindowMutex;
    QPointer<QQuickWindow> mRenderWindow;
    QQuickWindow *mRenderWindowIdentity;
    HeldFrame mCurrentFrame;
    QVector<HeldFrame> mDeferredFrames;

    Q_DISABLE_COPY(TextureFrameLease)
};

class PendingTextureCleanup final
{
public:
    PendingTextureCleanup(
            QMozExtTexture *texture,
            quint64 leaseId,
            quint64 consumerId,
            const QMozTextureCleanupComplete &complete)
        : mTexture(texture)
        , mLeaseId(leaseId)
        , mConsumerId(consumerId)
        , mFrameReleased(false)
        , mCompleted(false)
    {
        Q_ASSERT(mLeaseId);
        Q_ASSERT(mConsumerId);
        if (complete) {
            mComplete.append(complete);
        }
    }

    QMozExtTexture *take()
    {
        QMutexLocker lock(&mMutex);
        QMozExtTexture * const texture = mTexture;
        mTexture = nullptr;
        return texture;
    }

    quint64 leaseId() const
    {
        return mLeaseId;
    }

    quint64 consumerId() const
    {
        return mConsumerId;
    }

    void addComplete(const QMozTextureCleanupComplete &complete)
    {
        if (!complete) {
            return;
        }
        bool completed = false;
        {
            QMutexLocker lock(&mMutex);
            completed = mCompleted;
            if (!completed) {
                mComplete.append(complete);
            }
        }
        if (completed) {
            scheduleTextureCleanupComplete(complete);
        }
    }

    void markFrameReleased()
    {
        QMutexLocker lock(&mMutex);
        mFrameReleased = true;
    }

    bool complete()
    {
        QVector<QMozTextureCleanupComplete> complete;
        {
            QMutexLocker lock(&mMutex);
            if (!mFrameReleased) {
                return false;
            }
            if (mCompleted) {
                return true;
            }
            complete.swap(mComplete);
            mCompleted = true;
        }
        for (const QMozTextureCleanupComplete &callback : complete) {
            scheduleTextureCleanupComplete(callback);
        }
        return true;
    }

private:
    QMutex mMutex;
    QMozExtTexture *mTexture;
    const quint64 mLeaseId;
    const quint64 mConsumerId;
    QVector<QMozTextureCleanupComplete> mComplete;
    bool mFrameReleased;
    bool mCompleted;

    Q_DISABLE_COPY(PendingTextureCleanup)
};

void finishPendingTextureCleanup(
        QQuickWindow *window,
        const QSharedPointer<PendingTextureCleanup> &cleanup,
        bool completed);

void finishCompletedTextureCleanup(
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
        const bool completed = mCleanup->complete();
        finishPendingTextureCleanup(mWindow, mCleanup, completed);
    }

private:
    QQuickWindow * const mWindow;
    const QSharedPointer<PendingTextureCleanup> mCleanup;

    Q_DISABLE_COPY(TextureCleanupJob)
};

struct TextureLeaseRegistry
{
    QMutex mutex;
    quint64 nextConsumerId = 1;
    quint64 nextLeaseId = 1;
    QHash<const void *, quint64> consumerIds;
    QSet<quint64> registeredConsumers;
    QSet<quint64> drainingConsumers;
    QHash<QMozExtTexture *, QSharedPointer<TextureFrameLease>> leases;
    QHash<quint64, QWeakPointer<TextureFrameLease>> consumerLeases;
    QSet<quint64> invalidatedConsumers;
    QHash<QQuickWindow *, QVector<QSharedPointer<PendingTextureCleanup>>>
            pendingCleanups;
    QHash<quint64, QWeakPointer<PendingTextureCleanup>> cleanups;
    QHash<QMozExtTexture *, QSharedPointer<PendingTextureCleanup>>
            destructorCleanups;
    QHash<quint64, QSharedPointer<PendingTextureCleanup>>
            invalidatingCleanups;
    QSet<QQuickWindow *> watchedWindows;
    QSet<QQuickWindow *> invalidatingWindows;
    struct RetainedLease
    {
        QSharedPointer<TextureFrameLease> lease;
        QSharedPointer<PendingTextureCleanup> cleanup;
    };
    QVector<RetainedLease> retainedLeases;
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

void markConsumerInvalidated(
        TextureLeaseRegistry *registry,
        const QSharedPointer<TextureFrameLease> &lease)
{
    const quint64 consumerId = lease->consumerId();
    if (registry->registeredConsumers.contains(consumerId)
            && registry->consumerLeases.value(consumerId).toStrongRef()
                    == lease) {
        registry->invalidatedConsumers.insert(consumerId);
    }
}

void finishPendingTextureCleanup(
        QQuickWindow *window,
        const QSharedPointer<PendingTextureCleanup> &cleanup,
        bool completed)
{
    TextureLeaseRegistry * const registry = textureLeaseRegistry();
    QMutexLocker lock(&registry->mutex);
    auto found = registry->pendingCleanups.find(window);
    if (found != registry->pendingCleanups.end()) {
        found.value().removeAll(cleanup);
        if (found.value().isEmpty()) {
            registry->pendingCleanups.erase(found);
        }
    }
    if (completed
            && registry->cleanups.value(cleanup->leaseId()).toStrongRef()
                    == cleanup) {
        registry->cleanups.remove(cleanup->leaseId());
    }
}

void finishCompletedTextureCleanup(
        const QSharedPointer<PendingTextureCleanup> &cleanup)
{
    if (!cleanup) {
        return;
    }

    TextureLeaseRegistry * const registry = textureLeaseRegistry();
    QMutexLocker lock(&registry->mutex);
    if (registry->cleanups.value(cleanup->leaseId()).toStrongRef()
            == cleanup) {
        registry->cleanups.remove(cleanup->leaseId());
    }
    if (registry->invalidatingCleanups.value(cleanup->leaseId())
            == cleanup) {
        registry->invalidatingCleanups.remove(cleanup->leaseId());
    }
    for (auto found = registry->pendingCleanups.begin();
            found != registry->pendingCleanups.end();) {
        found.value().removeAll(cleanup);
        if (found.value().isEmpty()) {
            found = registry->pendingCleanups.erase(found);
        } else {
            ++found;
        }
    }
    registry->orphanedCleanups.removeAll(cleanup);
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
    QVector<TextureLeaseRegistry::RetainedLease> leases;
    {
        QMutexLocker lock(&registry->mutex);
        leases.swap(registry->retainedLeases);
    }

    QVector<TextureLeaseRegistry::RetainedLease> retained;
    for (const TextureLeaseRegistry::RetainedLease &entry : leases) {
        if (!entry.lease->releaseAll()) {
            retained.append(entry);
        } else if (entry.cleanup) {
            entry.cleanup->markFrameReleased();
            if (entry.cleanup->complete()) {
                finishCompletedTextureCleanup(entry.cleanup);
            }
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
    struct ActiveTexture
    {
        QMozExtTexture *texture;
        QSharedPointer<TextureFrameLease> lease;
        QSharedPointer<PendingTextureCleanup> cleanup;
    };
    QVector<ActiveTexture> active;
    QSet<quint64> pendingLeases;
    {
        QMutexLocker lock(&registry->mutex);
        registry->invalidatingWindows.insert(window);
        pending = registry->pendingCleanups.take(window);
        for (const QSharedPointer<PendingTextureCleanup> &cleanup : pending) {
            pendingLeases.insert(cleanup->leaseId());
        }
        for (auto it = registry->leases.constBegin();
                it != registry->leases.constEnd(); ++it) {
            if (!it.value()->belongsToWindow(window)) {
                continue;
            }

            QSharedPointer<PendingTextureCleanup> cleanup;
            if (!pendingLeases.contains(it.value()->leaseId())) {
                cleanup = registry->cleanups.value(
                        it.value()->leaseId()).toStrongRef();
                if (!cleanup) {
                    cleanup.reset(new PendingTextureCleanup(
                            it.key(), it.value()->leaseId(),
                            it.value()->consumerId(),
                            QMozTextureCleanupComplete()));
                    registry->cleanups.insert(
                            it.value()->leaseId(), cleanup.toWeakRef());
                }
                registry->invalidatingCleanups.insert(
                        it.value()->leaseId(), cleanup);
            }
            markConsumerInvalidated(registry, it.value());
            active.append({ it.key(), it.value(), cleanup });
        }
    }

    for (const QSharedPointer<PendingTextureCleanup> &cleanup : pending) {
        QMozExtTexture * const texture = cleanup->take();
        if (texture) {
            delete texture;
        }
        if (cleanup->complete()) {
            finishCompletedTextureCleanup(cleanup);
        }
    }
    for (const auto &entry : active) {
        if (pendingLeases.contains(entry.lease->leaseId())) {
            continue;
        }
        delete entry.cleanup->take();
        if (entry.cleanup->complete()) {
            finishCompletedTextureCleanup(entry.cleanup);
        }
        {
            QMutexLocker lock(&registry->mutex);
            if (registry->invalidatingCleanups.value(
                    entry.lease->leaseId())
                    == entry.cleanup) {
                registry->invalidatingCleanups.remove(
                        entry.lease->leaseId());
            }
        }
    }
    retryRetainedTextureFrameLeases();
}

void orphanTextureFrameWindow(QQuickWindow *window)
{
    if (!window) {
        return;
    }

    TextureLeaseRegistry * const registry = textureLeaseRegistry();
    QVector<QPair<QMozExtTexture *, QSharedPointer<TextureFrameLease>>> active;
    {
        QMutexLocker lock(&registry->mutex);
        registry->watchedWindows.remove(window);
        registry->invalidatingWindows.remove(window);
        registry->orphanedCleanups +=
                registry->pendingCleanups.take(window);
        for (auto it = registry->leases.constBegin();
                it != registry->leases.constEnd(); ++it) {
            if (it.value()->orphanWindow(window)) {
                markConsumerInvalidated(registry, it.value());
                active.append(qMakePair(
                        it.key(), it.value()));
            }
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

quint64 registerTextureFrameConsumer(const void *consumer)
{
    if (!consumer) {
        return 0;
    }

    TextureLeaseRegistry * const registry = textureLeaseRegistry();
    QMutexLocker lock(&registry->mutex);
    quint64 consumerId = registry->nextConsumerId++;
    if (consumerId == 0) {
        consumerId = registry->nextConsumerId++;
    }
    registry->consumerIds.insert(consumer, consumerId);
    registry->registeredConsumers.insert(consumerId);
    return consumerId;
}

quint64 textureFrameConsumerId(const void *consumer)
{
    if (!consumer) {
        return 0;
    }
    TextureLeaseRegistry * const registry = textureLeaseRegistry();
    QMutexLocker lock(&registry->mutex);
    return registry->consumerIds.value(consumer);
}

void unregisterTextureFrameConsumer(
        const void *consumer, quint64 consumerId)
{
    if (!consumer || consumerId == 0) {
        return;
    }

    TextureLeaseRegistry * const registry = textureLeaseRegistry();
    QMutexLocker lock(&registry->mutex);
    if (registry->consumerIds.value(consumer) == consumerId) {
        registry->consumerIds.remove(consumer);
        registry->registeredConsumers.remove(consumerId);
        registry->drainingConsumers.remove(consumerId);
        registry->invalidatedConsumers.remove(consumerId);
        if (registry->consumerLeases.value(consumerId).isNull()) {
            registry->consumerLeases.remove(consumerId);
        }
    }
}

bool attachTextureFrameLease(
        QMozExtTexture *texture, QMozWindow *window,
        const void *frameConsumer, quint64 consumerId,
        QQuickWindow *renderWindow)
{
    TextureLeaseRegistry * const registry = textureLeaseRegistry();
    const QSharedPointer<QMozFrameStream> stream =
            windowFrameStream(window);
    if (!texture || !frameConsumer || consumerId == 0
            || !renderWindow || !stream) {
        return false;
    }

    QSharedPointer<TextureFrameLease> lease;
    {
        QMutexLocker lock(&registry->mutex);
        if (registry->leases.contains(texture)
                || registry->consumerIds.value(frameConsumer)
                        != consumerId
                || registry->drainingConsumers.contains(consumerId)) {
            return false;
        }
        quint64 leaseId = registry->nextLeaseId++;
        if (leaseId == 0) {
            leaseId = registry->nextLeaseId++;
        }
        lease.reset(new TextureFrameLease(
                stream, leaseId, consumerId, frameConsumer, renderWindow));
        registry->leases.insert(texture, lease);
        registry->consumerLeases.insert(consumerId, lease.toWeakRef());
        registry->invalidatedConsumers.remove(consumerId);
    }
    watchTextureFrameWindow(renderWindow);
    return true;
}

bool takeTextureFrameInvalidation(quint64 consumerId)
{
    if (consumerId == 0) {
        return false;
    }
    TextureLeaseRegistry * const registry = textureLeaseRegistry();
    QMutexLocker lock(&registry->mutex);
    return registry->invalidatedConsumers.remove(consumerId) != 0;
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

static bool scheduleTextureFrameCleanupExact(
        QMozExtTexture *texture,
        const QMozTextureCleanupComplete &complete,
        const QSharedPointer<TextureFrameLease> &expectedLease)
{
    TextureLeaseRegistry * const registry = textureLeaseRegistry();
    if (!texture) {
        return false;
    }

    QSharedPointer<PendingTextureCleanup> cleanup;
    QSharedPointer<PendingTextureCleanup> existingCleanup;
    QQuickWindow *window = nullptr;
    bool invalidating = false;
    {
        QMutexLocker lock(&registry->mutex);
        const QSharedPointer<TextureFrameLease> lease =
                registry->leases.value(texture);
        if (!lease || (expectedLease && lease != expectedLease)) {
            return false;
        }
        existingCleanup = registry->cleanups.value(
                lease->leaseId()).toStrongRef();
        if (!existingCleanup) {
            cleanup.reset(new PendingTextureCleanup(
                    texture, lease->leaseId(), lease->consumerId(), complete));
            window = lease->renderWindow();
            if (!window) {
                markConsumerInvalidated(registry, lease);
                registry->cleanups.insert(
                        lease->leaseId(), cleanup.toWeakRef());
                registry->orphanedCleanups.append(cleanup);
                qCCritical(lcEmbedLiteExt)
                        << "Retaining platform frame without scene graph window";
                return true;
            }
            if (registry->invalidatingWindows.contains(window)) {
                invalidating = true;
                markConsumerInvalidated(registry, lease);
                // The invalidation handler claimed every texture for this
                // window before taking its active snapshot. It will perform
                // the delete.
                existingCleanup = registry->cleanups.value(
                        lease->leaseId()).toStrongRef();
                if (!existingCleanup) {
                    registry->cleanups.insert(
                            lease->leaseId(), cleanup.toWeakRef());
                    registry->invalidatingCleanups.insert(
                            lease->leaseId(), cleanup);
                }
            } else {
                if (registry->leases.value(texture) != lease
                        || !lease->belongsToWindow(window)) {
                    return false;
                }
                registry->cleanups.insert(
                        lease->leaseId(), cleanup.toWeakRef());
                registry->pendingCleanups[window].append(cleanup);
                markConsumerInvalidated(registry, lease);
            }
        }
    }
    if (existingCleanup) {
        existingCleanup->addComplete(complete);
        return true;
    }
    if (invalidating) {
        return true;
    }
    window->scheduleRenderJob(new TextureCleanupJob(window, cleanup),
                              QQuickWindow::AfterRenderingStage);
    window->update();
    return true;
}

bool scheduleTextureFrameCleanup(
        QMozExtTexture *texture,
        const QMozTextureCleanupComplete &complete)
{
    return scheduleTextureFrameCleanupExact(
            texture, complete, QSharedPointer<TextureFrameLease>());
}

bool scheduleTextureFrameCleanupForConsumer(
        quint64 consumerId,
        const QMozTextureCleanupComplete &complete)
{
    if (consumerId == 0) {
        return false;
    }

    TextureLeaseRegistry * const registry = textureLeaseRegistry();
    QMozExtTexture *texture = nullptr;
    QSharedPointer<TextureFrameLease> current;
    {
        QMutexLocker lock(&registry->mutex);
        current = registry->consumerLeases.value(consumerId).toStrongRef();
        for (auto it = registry->leases.constBegin();
                it != registry->leases.constEnd(); ++it) {
            if (current && it.value() == current) {
                texture = it.key();
                break;
            }
        }
    }
    if (texture && scheduleTextureFrameCleanupExact(
            texture, complete, current)) {
        return true;
    }
    return waitForTextureFrameCleanup(consumerId, complete);
}

bool drainTextureFramesForConsumer(
        quint64 consumerId,
        const QMozTextureCleanupComplete &complete)
{
    if (consumerId == 0 || !complete) {
        return false;
    }

    TextureLeaseRegistry * const registry = textureLeaseRegistry();
    QVector<QPair<QMozExtTexture *, QSharedPointer<TextureFrameLease>>> active;
    {
        QMutexLocker lock(&registry->mutex);
        if (registry->registeredConsumers.contains(consumerId)) {
            registry->drainingConsumers.insert(consumerId);
        }
        for (auto it = registry->leases.constBegin();
                it != registry->leases.constEnd(); ++it) {
            if (it.value()->consumerId() == consumerId) {
                active.append(qMakePair(it.key(), it.value()));
            }
        }
    }

    // Capture and claim every exact generation which was active when the
    // drain began. The draining flag prevents a later attachment from being
    // mistaken for this window's outstanding texture.
    for (const auto &entry : active) {
        scheduleTextureFrameCleanupExact(
                entry.first, QMozTextureCleanupComplete(), entry.second);
    }
    return waitForTextureFrameCleanup(consumerId, complete);
}

void finishTextureFrameConsumerDrain(quint64 consumerId)
{
    if (consumerId == 0) {
        return;
    }

    TextureLeaseRegistry * const registry = textureLeaseRegistry();
    QMutexLocker lock(&registry->mutex);
    registry->drainingConsumers.remove(consumerId);
}

bool hasTextureFrameLease(quint64 consumerId)
{
    if (consumerId == 0) {
        return false;
    }
    TextureLeaseRegistry * const registry = textureLeaseRegistry();
    QMutexLocker lock(&registry->mutex);
    return !registry->consumerLeases.value(consumerId).isNull();
}

bool releaseTexturePlatformFrame(QMozExtTexture *texture)
{
    TextureLeaseRegistry * const registry = textureLeaseRegistry();
    if (!registry || !texture) {
        return true;
    }

    QSharedPointer<TextureFrameLease> lease;
    {
        QMutexLocker lock(&registry->mutex);
        lease = registry->leases.value(texture);
    }
    return !lease || lease->releaseAll();
}

bool releaseTexturePlatformFrames(QMozExtTexture *texture)
{
    TextureLeaseRegistry * const registry = textureLeaseRegistry();
    if (!registry || !texture) {
        return true;
    }

    QSharedPointer<TextureFrameLease> lease;
    QSharedPointer<PendingTextureCleanup> cleanup;
    {
        QMutexLocker lock(&registry->mutex);
        lease = registry->leases.take(texture);
        if (lease) {
            cleanup = registry->cleanups.value(
                    lease->leaseId()).toStrongRef();
            bool completeAfterDestructor = false;
            if (cleanup) {
                QMozExtTexture * const claimedTexture = cleanup->take();
                Q_ASSERT(!claimedTexture || claimedTexture == texture);
                completeAfterDestructor = claimedTexture;
            } else {
                cleanup.reset(new PendingTextureCleanup(
                        nullptr, lease->leaseId(), lease->consumerId(),
                        QMozTextureCleanupComplete()));
                registry->cleanups.insert(
                        lease->leaseId(), cleanup.toWeakRef());
                completeAfterDestructor = true;
            }
            if (completeAfterDestructor) {
                registry->destructorCleanups.insert(texture, cleanup);
            }
            if (registry->consumerLeases.value(lease->consumerId())
                    .toStrongRef() == lease) {
                registry->consumerLeases.remove(lease->consumerId());
                if (!registry->registeredConsumers.contains(
                        lease->consumerId())) {
                    registry->invalidatedConsumers.remove(
                            lease->consumerId());
                    registry->drainingConsumers.remove(
                            lease->consumerId());
                }
            }
        }
    }
    if (!lease || lease->releaseAll()) {
        if (cleanup) {
            cleanup->markFrameReleased();
        }
        return true;
    }

    // Without a current compatible GL context there is no truthful
    // NoHandle release. Retain the lease and its surface instead of allowing
    // Gecko to recycle an image that the GPU may still sample.
    QMutexLocker lock(&registry->mutex);
    registry->retainedLeases.append({ lease, cleanup });
    qCCritical(lcEmbedLiteExt)
            << "Retaining platform frame after unsafe texture teardown";
    return false;
}

void finishTexturePlatformFrameDestruction(QMozExtTexture *texture)
{
    if (!texture) {
        return;
    }

    TextureLeaseRegistry * const registry = textureLeaseRegistry();
    QSharedPointer<PendingTextureCleanup> cleanup;
    {
        QMutexLocker lock(&registry->mutex);
        cleanup = registry->destructorCleanups.take(texture);
    }
    if (cleanup && cleanup->complete()) {
        finishCompletedTextureCleanup(cleanup);
    }
}

void scheduleTextureCleanupComplete(
        const QMozTextureCleanupComplete &complete)
{
    if (!complete) {
        return;
    }
    QObject * const dispatcher = QCoreApplication::instance();
    if (dispatcher && QThread::currentThread() != dispatcher->thread()) {
        QTimer::singleShot(0, dispatcher, complete);
    } else {
        complete();
    }
}

bool waitForTextureFrameCleanup(
        quint64 consumerId,
        const QMozTextureCleanupComplete &complete)
{
    if (consumerId == 0 || !complete) {
        return false;
    }

    TextureLeaseRegistry * const registry = textureLeaseRegistry();
    QVector<QSharedPointer<PendingTextureCleanup>> cleanups;
    {
        QMutexLocker lock(&registry->mutex);
        for (auto it = registry->cleanups.constBegin();
                it != registry->cleanups.constEnd(); ++it) {
            const QSharedPointer<PendingTextureCleanup> cleanup =
                    it.value().toStrongRef();
            if (cleanup && cleanup->consumerId() == consumerId) {
                cleanups.append(cleanup);
            }
        }
    }
    if (cleanups.isEmpty()) {
        return false;
    }

    struct WaitState
    {
        int remaining;
        QMozTextureCleanupComplete complete;
    };
    const QSharedPointer<WaitState> state(
            new WaitState { cleanups.size(), complete });
    for (const QSharedPointer<PendingTextureCleanup> &cleanup : cleanups) {
        cleanup->addComplete([state]() {
            if (--state->remaining == 0 && state->complete) {
                const QMozTextureCleanupComplete complete =
                        state->complete;
                state->complete = QMozTextureCleanupComplete();
                complete();
            }
        });
    }
    return true;
}

} // namespace QtMoz

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "qmozframestream_p.h"

#include <QHash>
#include <QMutexLocker>
#include <QWeakPointer>

namespace {

struct FrameStreamRegistry
{
    QMutex mutex;
    QHash<QMozWindow *, QSharedPointer<QtMoz::QMozFrameStream>> streams;
};

Q_GLOBAL_STATIC(FrameStreamRegistry, frameStreamRegistry)

} // namespace

namespace QtMoz {

QMozFrameStream::QMozFrameStream(
        const QSharedPointer<QMozSurface> &surface)
    : mSurface(surface)
    , mConsumer(nullptr)
    , mPendingToken({ 0, 0 })
    , mStarting(false)
    , mEnabled(false)
    , mStopped(false)
{
    Q_ASSERT(!mSurface.isNull());
}

bool QMozFrameStream::start()
{
    QSharedPointer<QMozSurface> surface;
    {
        QMutexLocker lock(&mMutex);
        if (mStopped) {
            return false;
        }
        if (mEnabled) {
            return true;
        }
        if (mStarting) {
            return false;
        }
        mStarting = true;
        surface = mSurface;
    }

    const bool enabled = surface
            && surface->setPlatformFrameDeliveryEnabled(true);
    bool scheduleUpdate = false;
    {
        QMutexLocker lock(&mMutex);
        mStarting = false;
        if (enabled && !mStopped) {
            mEnabled = true;
            scheduleUpdate = mConsumer != nullptr;
        }
    }

    if (scheduleUpdate) {
        surface->scheduleUpdate();
    }
    return enabled;
}

void QMozFrameStream::backendDestroyed()
{
    QMutexLocker lock(&mMutex);
    mStarting = false;
    mEnabled = false;
    mStopped = true;
    mConsumer = nullptr;
    mUpdateCallback = QMozFrameConsumerUpdate();
    mPendingToken = { 0, 0 };
}

void QMozFrameStream::setConsumer(
        const void *consumer,
        const QMozFrameConsumerUpdate &updateCallback)
{
    if (!consumer || !updateCallback) {
        return;
    }

    QSharedPointer<QMozSurface> surface;
    bool scheduleUpdate = false;
    {
        QMutexLocker lock(&mMutex);
        if (mStopped) {
            return;
        }
        if (mConsumer != consumer) {
            mPendingToken = { 0, 0 };
        }
        mConsumer = consumer;
        mUpdateCallback = updateCallback;
        scheduleUpdate = mEnabled;
        surface = mSurface;
    }

    if (scheduleUpdate && surface) {
        surface->scheduleUpdate();
    }
}

void QMozFrameStream::clearConsumer(const void *consumer)
{
    QMutexLocker lock(&mMutex);
    if (mConsumer == consumer) {
        mConsumer = nullptr;
        mUpdateCallback = QMozFrameConsumerUpdate();
        mPendingToken = { 0, 0 };
    }
}

void QMozFrameStream::clearPendingFrame()
{
    QMutexLocker lock(&mMutex);
    mPendingToken = { 0, 0 };
}

bool QMozFrameStream::takePendingFrame(
        const void *consumer, QMozSurfaceFrameToken *token)
{
    if (!consumer || !token) {
        return false;
    }

    QMutexLocker lock(&mMutex);
    if (!mEnabled || mStopped || mConsumer != consumer
            || !mPendingToken.isValid()) {
        return false;
    }
    *token = mPendingToken;
    mPendingToken = { 0, 0 };
    return true;
}

void QMozFrameStream::restorePendingFrame(
        const void *consumer, const QMozSurfaceFrameToken &token)
{
    if (!consumer || !token.isValid()) {
        return;
    }

    QMutexLocker lock(&mMutex);
    if (mEnabled && !mStopped && mConsumer == consumer
            && !mPendingToken.isValid()) {
        mPendingToken = token;
    }
}

bool QMozFrameStream::enabled() const
{
    QMutexLocker lock(&mMutex);
    return mEnabled && !mStopped;
}

QSharedPointer<QMozSurface> QMozFrameStream::surface() const
{
    QMutexLocker lock(&mMutex);
    return mSurface;
}

void QMozFrameStream::frameReady(const QMozSurfaceFrameToken &token)
{
    QMozFrameConsumerUpdate updateCallback;
    {
        QMutexLocker lock(&mMutex);
        if (!mEnabled || mStopped || !mConsumer || !token.isValid()) {
            return;
        }
        mPendingToken = token;
        updateCallback = mUpdateCallback;
    }

    if (updateCallback) {
        updateCallback();
    }
}

void QMozFrameStream::frameDeliveryStopped()
{
    QMutexLocker lock(&mMutex);
    mStarting = false;
    mEnabled = false;
    mStopped = true;
    mPendingToken = { 0, 0 };
}

bool installWindowFrameStream(
        QMozWindow *window, const QSharedPointer<QMozSurface> &surface)
{
    FrameStreamRegistry * const registry = frameStreamRegistry();
    if (!registry || !window || surface.isNull()) {
        return false;
    }

    const QSharedPointer<QMozFrameStream> stream(
            new QMozFrameStream(surface));
    {
        QMutexLocker lock(&registry->mutex);
        if (registry->streams.contains(window)) {
            return false;
        }
        registry->streams.insert(window, stream);
    }

    const QWeakPointer<QMozFrameStream> weakStream(stream);
    if (!surface->setPlatformFrameCallbacks(
            [weakStream](const QMozSurfaceFrameToken &token) {
        const QSharedPointer<QMozFrameStream> strongStream =
                weakStream.toStrongRef();
        if (strongStream) {
            strongStream->frameReady(token);
        }
    }, [weakStream]() {
        const QSharedPointer<QMozFrameStream> strongStream =
                weakStream.toStrongRef();
        if (strongStream) {
            strongStream->frameDeliveryStopped();
        }
    })) {
        QMutexLocker lock(&registry->mutex);
        if (registry->streams.value(window) == stream) {
            registry->streams.remove(window);
        }
        return false;
    }
    return true;
}

QSharedPointer<QMozFrameStream> windowFrameStream(QMozWindow *window)
{
    FrameStreamRegistry * const registry = frameStreamRegistry();
    if (!registry || !window) {
        return QSharedPointer<QMozFrameStream>();
    }

    QMutexLocker lock(&registry->mutex);
    return registry->streams.value(window);
}

QSharedPointer<QMozFrameStream> takeWindowFrameStream(QMozWindow *window)
{
    FrameStreamRegistry * const registry = frameStreamRegistry();
    if (!registry || !window) {
        return QSharedPointer<QMozFrameStream>();
    }

    QMutexLocker lock(&registry->mutex);
    return registry->streams.take(window);
}

bool startWindowFrameStream(QMozWindow *window)
{
    const QSharedPointer<QMozFrameStream> stream =
            windowFrameStream(window);
    return stream && stream->start();
}

void setWindowFrameConsumer(
        QMozWindow *window, const void *consumer,
        const QMozFrameConsumerUpdate &updateCallback)
{
    const QSharedPointer<QMozFrameStream> stream =
            windowFrameStream(window);
    if (stream) {
        stream->setConsumer(consumer, updateCallback);
    }
}

void clearWindowFrameConsumer(QMozWindow *window, const void *consumer)
{
    const QSharedPointer<QMozFrameStream> stream =
            windowFrameStream(window);
    if (stream) {
        stream->clearConsumer(consumer);
    }
}

void clearWindowPendingFrame(QMozWindow *window)
{
    const QSharedPointer<QMozFrameStream> stream =
            windowFrameStream(window);
    if (stream) {
        stream->clearPendingFrame();
    }
}

} // namespace QtMoz

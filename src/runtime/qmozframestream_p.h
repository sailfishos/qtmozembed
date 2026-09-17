/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef QMOZFRAMESTREAM_P_H
#define QMOZFRAMESTREAM_P_H

#include "qmozsurface_p.h"

#include <QMutex>
#include <QSharedPointer>

#include <functional>

class QMozWindow;

namespace QtMoz {

using QMozFrameConsumerUpdate = std::function<void()>;

class Q_DECL_HIDDEN QMozFrameStream
{
public:
    explicit QMozFrameStream(const QSharedPointer<QMozSurface> &surface);

    bool start();
    void backendDestroyed();
    void setConsumer(const void *consumer,
                     const QMozFrameConsumerUpdate &updateCallback);
    void clearConsumer(const void *consumer);
    void clearPendingFrame();

    bool takePendingFrame(const void *consumer,
                          QMozSurfaceFrameToken *token);
    void restorePendingFrame(const void *consumer,
                             const QMozSurfaceFrameToken &token);

    bool enabled() const;
    QSharedPointer<QMozSurface> surface() const;

    void frameReady(const QMozSurfaceFrameToken &token);
    void frameDeliveryStopped();

private:
    QSharedPointer<QMozSurface> mSurface;
    mutable QMutex mMutex;
    const void *mConsumer;
    QMozFrameConsumerUpdate mUpdateCallback;
    QMozSurfaceFrameToken mPendingToken;
    bool mStarting;
    bool mEnabled;
    bool mStopped;

    Q_DISABLE_COPY(QMozFrameStream)
};

Q_DECL_HIDDEN bool installWindowFrameStream(
        QMozWindow *window, const QSharedPointer<QMozSurface> &surface);
Q_DECL_HIDDEN QSharedPointer<QMozFrameStream> windowFrameStream(
        QMozWindow *window);
Q_DECL_HIDDEN QSharedPointer<QMozFrameStream> takeWindowFrameStream(
        QMozWindow *window);
Q_DECL_HIDDEN bool startWindowFrameStream(QMozWindow *window);
Q_DECL_HIDDEN void setWindowFrameConsumer(
        QMozWindow *window, const void *consumer,
        const QMozFrameConsumerUpdate &updateCallback);
Q_DECL_HIDDEN void clearWindowFrameConsumer(
        QMozWindow *window, const void *consumer);
Q_DECL_HIDDEN void clearWindowPendingFrame(QMozWindow *window);

} // namespace QtMoz

#endif // QMOZFRAMESTREAM_P_H

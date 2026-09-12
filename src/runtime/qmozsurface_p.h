/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef QMOZSURFACE_P_H
#define QMOZSURFACE_P_H

#include <QSharedPointer>
#include <QSize>
#include <QtGlobal>

#include <functional>

class QMozWindow;

enum class QMozSurfaceRotation {
    Rotation0,
    Rotation90,
    Rotation180,
    Rotation270
};

enum class QMozSurfaceTextureTarget {
    Texture2D,
    ExternalOES
};

struct QMozSurfaceImage final
{
    void *handle;
    QSize size;
    QMozSurfaceTextureTarget textureTarget;
};

using QMozSurfaceImageCallback =
        std::function<void(const QMozSurfaceImage &)>;

struct QMozSurfaceFrameToken final
{
    quint64 epoch;
    quint64 sequence;

    bool isValid() const
    {
        return epoch != 0 && sequence != 0;
    }
};

enum class QMozSurfaceFrameFenceType {
    NoHandle,
    EGLSync
};

struct QMozSurfaceFrame final
{
    QMozSurfaceFrameToken token;
    QMozSurfaceImage image;
    QMozSurfaceFrameFenceType releaseFenceType;
};

struct QMozSurfaceFrameRelease final
{
    QMozSurfaceFrameToken token;
    QMozSurfaceFrameFenceType fenceType;
    void *fence;
};

using QMozSurfaceFrameCallback =
        std::function<bool(const QMozSurfaceFrame &)>;
using QMozSurfaceFrameReadyCallback =
        std::function<void(const QMozSurfaceFrameToken &)>;
using QMozSurfaceFrameDeliveryStoppedCallback = std::function<void()>;

class Q_DECL_HIDDEN QMozSurface
{
public:
    virtual ~QMozSurface();

    virtual void requestDestroy() = 0;
    virtual void backendDestroyed() = 0;
    virtual bool setSize(const QSize &size) = 0;
    virtual bool setContentOrientation(QMozSurfaceRotation rotation) = 0;
    virtual bool withPlatformImage(
            const QMozSurfaceImageCallback &callback) = 0;
    // Notifications are marshalled to the surface owner thread. The frame
    // image itself is acquired synchronously on the caller's render thread.
    virtual bool setPlatformFrameCallbacks(
            const QMozSurfaceFrameReadyCallback &readyCallback,
            const QMozSurfaceFrameDeliveryStoppedCallback &stoppedCallback) = 0;
    virtual bool setPlatformFrameDeliveryEnabled(bool enabled) = 0;
    // The image handle is borrowed for the callback. Returning true retains
    // the token lease until releasePlatformFrame succeeds.
    virtual bool acquirePlatformFrame(
            const QMozSurfaceFrameToken &token,
            const QMozSurfaceFrameCallback &callback) = 0;
    // Fence ownership transfers to the backend only when this returns true.
    // Release remains available while asynchronous surface teardown waits.
    virtual bool releasePlatformFrame(
            const QMozSurfaceFrameRelease &release) = 0;
    virtual bool clearPlatformImage() = 0;
    virtual bool suspendRendering() = 0;
    virtual bool resumeRendering() = 0;
    virtual bool scheduleUpdate() = 0;
};

namespace QtMoz {

Q_DECL_HIDDEN bool installWindowSurface(
        QMozWindow *window,
        const QSharedPointer<QMozSurface> &surface);
Q_DECL_HIDDEN QSharedPointer<QMozSurface> windowSurface(
        QMozWindow *window);
Q_DECL_HIDDEN QSharedPointer<QMozSurface> takeWindowSurface(
        QMozWindow *window);

} // namespace QtMoz

#endif // QMOZSURFACE_P_H

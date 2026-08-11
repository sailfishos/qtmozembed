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

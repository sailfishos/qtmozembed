/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef QMOZWINDOW_H
#define QMOZWINDOW_H

#include <QObject>
#include <QPointer>
#include <QRect>
#include <QScopedPointer>
#include <QSize>

#include <functional>

#include <EGL/egl.h>
#include <EGL/eglext.h>

class QMozWindowPrivate;

enum class QMozTextureTarget {
    Texture2D,
    ExternalOES
};

struct QMozEGLImage final
{
    EGLImageKHR image;
    QSize size;
    QMozTextureTarget textureTarget;
};

using QMozEGLImageCallback = std::function<void(const QMozEGLImage &)>;

class QMozWindow: public QObject
{
    Q_OBJECT

public:
    explicit QMozWindow(const QSize &size, QObject *parent = nullptr);
    ~QMozWindow();

    void reserve();
    void release();
    bool isReserved() const;
    void setSize(const QSize &size);
    QSize size() const;
    void setContentOrientation(Qt::ScreenOrientation);
    void setPrimaryOrientation(Qt::ScreenOrientation);
    Qt::ScreenOrientation contentOrientation() const;
    Qt::ScreenOrientation pendingOrientation() const;
    Qt::ScreenOrientation primaryOrientation() const;
    // The callback runs synchronously. The EGLImage handle is borrowed and
    // must be imported before the callback returns.
    bool withPlatformImage(const QMozEGLImageCallback &callback);
    void clearPlatformImage();
    void suspendRendering();
    void resumeRendering();
    void scheduleUpdate();
    bool setReadyToPaint(bool ready);
    bool readyToPaint() const;

    bool isCompositorCreated();

Q_SIGNALS:
    void pendingOrientationChanged(Qt::ScreenOrientation orientation);
    void orientationChangeFiltered(Qt::ScreenOrientation orientation);
    void initialized();
    void released();
    void drawOverlay(QRect);
    void compositorCreated();
    void compositingFinished();

protected:
    void timerEvent(QTimerEvent *event);

private:
    friend class QMozViewPrivate;
    friend class QMozWindowPrivate;

    QScopedPointer<QMozWindowPrivate> d;

    Q_DISABLE_COPY(QMozWindow)
};

#endif // QMOZWINDOW_H

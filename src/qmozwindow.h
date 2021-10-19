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

class QMozWindowPrivate;

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
    Qt::ScreenOrientation contentOrientation() const;
    Qt::ScreenOrientation pendingOrientation() const;
    void getPlatformImage(const std::function<void(void *image, int width, int height)> &callback);
    void suspendRendering();
    void resumeRendering();
    void scheduleUpdate();
    bool setReadyToPaint(bool ready);
    bool readyToPaint() const;

    bool isCompositorCreated();

Q_SIGNALS:
    void pendingOrientationChanged(Qt::ScreenOrientation orientation);
    void orientationChangeFiltered(Qt::ScreenOrientation orientation);
    void requestGLContext();
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

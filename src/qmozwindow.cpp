/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (c) 2015 - 2019 Jolla Ltd.
 * Copyright (c) 2019 Open Mobile Platform LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "qmozwindow.h"

#include "qmozcontext.h"
#include "qmozembedlog.h"
#include "qmozwindow_p.h"
#include "backends/embedlite/embedlitesurface_p.h"
#include "runtime/qmozchromehost_p.h"
#include "runtime/qmozframestream_p.h"
#include "runtime/qmozsurface_p.h"

#include "mozilla/embedlite/EmbedLiteApp.h"

using namespace mozilla::embedlite;

QMozWindow::QMozWindow(const QSize &size, QObject *parent)
    : QObject(parent)
    , d(new QMozWindowPrivate(*this, size))
{
    Q_ASSERT_X(!size.isEmpty(),
               "QMozWindow::QMozWindow",
               QString("Window size is empty, width = %1 and height = %2").arg(size.width()).arg(size.height()).toUtf8().constData());
}

QMozWindow::~QMozWindow()
{
    Q_ASSERT(!d->mReserved);
}

void QMozWindow::reserve()
{
    if (!d->mWindow && !d->mReserved) {
        QtMoz::clearChromeInitialized(this);
        const QSharedPointer<QMozSurface> surface =
                QtMoz::createEmbedLiteSurface(
                    QMozContext::instance()->GetApp(), d.data(),
                    [guardedWindow = QPointer<QMozWindow>(this)]() {
            if (guardedWindow) {
                QtMoz::markChromeInitializationFailed(
                        guardedWindow.data());
                qCWarning(lcEmbedLiteExt)
                        << "Gecko chrome window initialization failed";
                if (QtMoz::chromeQuickWindowShouldDelete(
                        QtMoz::isChromeQuickOwned(guardedWindow.data()),
                        true, guardedWindow->isReserved())) {
                    guardedWindow->deleteLater();
                }
            }
        });
        if (!QtMoz::installWindowSurface(this, surface)) {
            Q_ASSERT_X(false, "QMozWindow::reserve",
                       "A surface is already registered for this window");
            return;
        }

        // Install frame callbacks before CreateWindow can create its
        // compositor. Delivery itself starts later on the Qt owner thread.
        if (!QtMoz::installWindowFrameStream(this, surface)) {
            QtMoz::takeWindowSurface(this);
            return;
        }

        d->mWindow = QtMoz::reserveEmbedLiteSurface(
                surface, d->mSize, QtMoz::isChromeHosted(this),
                QtMoz::chromeInitialUrl(this));
        if (!d->mWindow) {
            QtMoz::takeWindowFrameStream(this);
            QtMoz::takeWindowSurface(this);
            return;
        }
        d->mReserved = true;
    }
}

void QMozWindow::release()
{
    if (d->mWindow) {
        EmbedLiteWindow * const window = d->mWindow;
        d->mWindow = nullptr;
        const QSharedPointer<QMozSurface> surface =
                QtMoz::windowSurface(this);
        if (surface) {
            surface->requestDestroy();
        } else {
            QMozContext::instance()->GetApp()->DestroyWindow(window);
        }
    }
}

bool QMozWindow::isReserved() const
{
    return d->mReserved;
}

void QMozWindow::setSize(const QSize &size)
{
    d->setSize(size);
}

QSize QMozWindow::size() const
{
    return d->mSize;
}

void QMozWindow::setContentOrientation(Qt::ScreenOrientation orientation)
{
    d->setContentOrientation(orientation);
}

void QMozWindow::setPrimaryOrientation(Qt::ScreenOrientation orientation)
{
    d->setPrimaryOrientation(orientation);
}

Qt::ScreenOrientation QMozWindow::contentOrientation() const
{
    return d->mOrientation;
}

Qt::ScreenOrientation QMozWindow::pendingOrientation() const
{
    return d->mPendingOrientation;
}

Qt::ScreenOrientation QMozWindow::primaryOrientation() const
{
    return d->mPrimaryOrientation;
}

bool QMozWindow::withPlatformImage(const QMozEGLImageCallback &callback)
{
    if (!callback) {
        return false;
    }

    const QSharedPointer<QMozSurface> surface =
            QtMoz::windowSurface(this);
    return surface && surface->withPlatformImage(
                [&](const QMozSurfaceImage &image) {
        QMozTextureTarget textureTarget;
        switch (image.textureTarget) {
        case QMozSurfaceTextureTarget::Texture2D:
            textureTarget = QMozTextureTarget::Texture2D;
            break;
        case QMozSurfaceTextureTarget::ExternalOES:
            textureTarget = QMozTextureTarget::ExternalOES;
            break;
        }

        callback({
            static_cast<EGLImageKHR>(image.handle),
            image.size,
            textureTarget
        });
    });
}

void QMozWindow::clearPlatformImage()
{
    QtMoz::clearWindowPendingFrame(this);
    const QSharedPointer<QMozSurface> surface =
            QtMoz::windowSurface(this);
    if (surface) {
        surface->clearPlatformImage();
    }
}

void QMozWindow::suspendRendering()
{
    const QSharedPointer<QMozSurface> surface =
            QtMoz::windowSurface(this);
    if (surface) {
        surface->suspendRendering();
    }
}

void QMozWindow::resumeRendering()
{
    const QSharedPointer<QMozSurface> surface =
            QtMoz::windowSurface(this);
    if (surface) {
        surface->resumeRendering();
    }
}

void QMozWindow::scheduleUpdate()
{
    const QSharedPointer<QMozSurface> surface =
            QtMoz::windowSurface(this);
    if (surface) {
        surface->scheduleUpdate();
    }
}

bool QMozWindow::setReadyToPaint(bool ready)
{
    return d->setReadyToPaint(ready);
}

bool QMozWindow::readyToPaint() const
{
    return d->PreRender();
}

bool QMozWindow::isCompositorCreated()
{
    return d->mCompositorCreated;
}

void QMozWindow::timerEvent(QTimerEvent *event)
{
    d->timerEvent(event);
}

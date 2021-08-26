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
#include "qmozwindow_p.h"

#include "mozilla/embedlite/EmbedLiteApp.h"
#include "mozilla/embedlite/EmbedLiteWindow.h"

using namespace mozilla::embedlite;

QMozWindow::QMozWindow(const QSize &size, QObject *parent)
    : QObject(parent)
    , d(new QMozWindowPrivate(*this, size))
{
    Q_ASSERT_X(!size.isEmpty(),
               "QMozWindow::QMozWindow",
               QString("Window size is empty, width = %1 and height = %2").arg(size.width()).arg(size.height()).toUtf8().constData());

    d->mWindow = QMozContext::instance()->GetApp()->CreateWindow(size.width(), size.height());
    d->mWindow->SetListener(d.data());
}

QMozWindow::~QMozWindow()
{
    d->mWindow->SetListener(nullptr);
    QMozContext::instance()->GetApp()->DestroyWindow(d->mWindow);
    d->mWindow = nullptr;
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

Qt::ScreenOrientation QMozWindow::contentOrientation() const
{
    return d->mOrientation;
}

Qt::ScreenOrientation QMozWindow::pendingOrientation() const
{
    return d->mPendingOrientation;
}

void QMozWindow::getPlatformImage(const std::function<void(void *image, int width, int height)> &callback)
{
    d->mWindow->GetPlatformImage(callback);
}

void QMozWindow::suspendRendering()
{
    // Revert this in context of JB#55286
#if 0
    d->mWindow->SuspendRendering();
#endif
}

void QMozWindow::resumeRendering()
{
    d->mWindow->ResumeRendering();
}

void QMozWindow::scheduleUpdate()
{
    d->mWindow->ScheduleUpdate();
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

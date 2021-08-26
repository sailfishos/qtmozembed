/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (c) 2015 - 2019 Jolla Ltd.
 * Copyright (c) 2019 Open Mobile Platform LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "qmozembedlog.h"
#include "qmozwindow_p.h"

#include "qmozwindow.h"

#include <QGuiApplication>
#include <dlfcn.h>
#include <EGL/egl.h>
#if defined(ENABLE_GLX)
#include <GL/glx.h>
#endif

#include <mozilla/embedlite/EmbedLiteWindow.h>

EGLContext (EGLAPIENTRY *_eglGetCurrentContext)(void) = nullptr;
EGLSurface (EGLAPIENTRY *_eglGetCurrentSurface)(EGLint readdraw) = nullptr;
EGLDisplay (EGLAPIENTRY *_eglGetCurrentDisplay)(void) = nullptr;

#if defined(ENABLE_GLX)
GLXContext (*_glxGetCurrentContext)(void) = nullptr;
GLXDrawable (*_glxGetCurrentDrawable)(void) = nullptr;
#endif

#ifndef MOZWINDOW_ORIENTATION_CHANGE_TIMEOUT
#define MOZWINDOW_ORIENTATION_CHANGE_TIMEOUT 500
#endif

namespace {

mozilla::embedlite::ScreenRotation QtToMozillaRotation(Qt::ScreenOrientation orientation)
{
    switch (orientation) {
    case Qt::PrimaryOrientation:
    case Qt::PortraitOrientation:
        return mozilla::embedlite::ROTATION_0;
    case Qt::LandscapeOrientation:
        return mozilla::embedlite::ROTATION_90;
    case Qt::InvertedLandscapeOrientation:
        return mozilla::embedlite::ROTATION_270;
    case Qt::InvertedPortraitOrientation:
        return mozilla::embedlite::ROTATION_180;
    default:
        Q_UNREACHABLE();
        return mozilla::embedlite::ROTATION_0;
    }
}

} // namespace

QMozWindowPrivate::QMozWindowPrivate(QMozWindow &window, const QSize &size)
    : q(window)
    , mWindow(nullptr)
    , mCompositorCreated(false)
    , mReadyToPaint(true)
    , mSize(size)
    , mOrientation(Qt::PrimaryOrientation)
    , mPendingOrientation(Qt::PrimaryOrientation)
    , mOrientationFilterTimer(0)
{
}

QMozWindowPrivate::~QMozWindowPrivate()
{
}

void QMozWindowPrivate::setSize(const QSize &size)
{
    if (size.isEmpty()) {
        qCDebug(lcEmbedLiteExt) << "Trying to set empty size: " << size;
    } else if (size != mSize) {
        mSize = size;
        mWindow->SetSize(size.width(), size.height());
    }
}

void QMozWindowPrivate::setContentOrientation(Qt::ScreenOrientation orientation)
{
    if (mOrientationFilterTimer > 0) {
        q.killTimer(mOrientationFilterTimer);
        mOrientationFilterTimer = 0;
    }

    if (mPendingOrientation != orientation) {
        mPendingOrientation = orientation;
        q.pendingOrientationChanged(mPendingOrientation);
    }
    mOrientationFilterTimer = q.startTimer(MOZWINDOW_ORIENTATION_CHANGE_TIMEOUT);
}

void QMozWindowPrivate::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == mOrientationFilterTimer) {
        q.killTimer(mOrientationFilterTimer);
        mOrientationFilterTimer = 0;
        if (mWindow) {
            if (mOrientation != mPendingOrientation) {
                mWindow->SetContentOrientation(QtToMozillaRotation(mPendingOrientation));
                mOrientation = mPendingOrientation;
            } else {
                q.orientationChangeFiltered(mOrientation);
            }
        }
        event->accept();
    }
}

bool QMozWindowPrivate::RequestGLContext(void *&context, void *&surface, void *&display)
{
    q.requestGLContext();

    QString platform = qApp->platformName().toLower();

    if (platform == "wayland" || platform == "wayland-egl" || platform == "eglfs") {
        getEGLContext(context, surface, display);
        return true;
    }
#if defined(ENABLE_GLX)
    if (platform == "xcb") {
        getGLXContext(context, surface);
        return true;
    }
#endif

    qCritical() << "Unsupported QPA platform type:" << platform;
    return false;
}

void QMozWindowPrivate::getEGLContext(void *&context, void *&surface, void *&display)
{
    if (!_eglGetCurrentContext || !_eglGetCurrentSurface) {
        void *handle = dlopen("libEGL.so.1", RTLD_LAZY);
        if (!handle)
            return;

        *(void **)(&_eglGetCurrentContext) = dlsym(handle, "eglGetCurrentContext");
        *(void **)(&_eglGetCurrentSurface) = dlsym(handle, "eglGetCurrentSurface");
        *(void **)(&_eglGetCurrentDisplay) = dlsym(handle, "eglGetCurrentDisplay");

        Q_ASSERT(_eglGetCurrentContext && _eglGetCurrentSurface && _eglGetCurrentDisplay);

        dlclose(handle);
    }

    surface = _eglGetCurrentSurface(EGL_DRAW);
    context = _eglGetCurrentContext();
    display = _eglGetCurrentDisplay();
}

#if defined(ENABLE_GLX)
void QMozWindowPrivate::getGLXContext(void *&context, void *&surface)
{
    if (!_glxGetCurrentContext || !_glxGetCurrentDrawable) {
        void *handle = dlopen("libGL.so.1", RTLD_LAZY);
        if (!handle)
            return;

        *(void **)(&_glxGetCurrentContext) = dlsym(handle, "glXGetCurrentContext");
        *(void **)(&_glxGetCurrentDrawable) = dlsym(handle, "glXGetCurrentDrawable");

        Q_ASSERT(_glxGetCurrentContext && _glxGetCurrentDrawable);

        dlclose(handle);
    }

    surface = reinterpret_cast<void *>(_glxGetCurrentDrawable());
    context = _glxGetCurrentContext();
}
#endif

bool QMozWindowPrivate::setReadyToPaint(bool ready)
{
    Q_UNUSED(ready);
    return true;
    // Revert this in context of JB#55286
#if 0
    QMutexLocker lock(&mReadyToPaintMutex);
    if (mReadyToPaint != ready) {
        mReadyToPaint = ready;
        return true;
    }
    return false;
#endif
}

void QMozWindowPrivate::WindowInitialized()
{
    q.initialized();
}

void QMozWindowPrivate::DrawUnderlay()
{
    q.drawUnderlay();
}

void QMozWindowPrivate::DrawOverlay(const nsIntRect &aRect)
{
    q.drawOverlay(QRect(aRect.x, aRect.y, aRect.width, aRect.height));
}

void QMozWindowPrivate::CompositorCreated()
{
    mCompositorCreated = true;
    q.compositorCreated();
}

void QMozWindowPrivate::CompositingFinished()
{
    q.compositingFinished();
}

bool QMozWindowPrivate::PreRender()
{
    return true;
#if 0
    QMutexLocker lock(&mReadyToPaintMutex);
    return mReadyToPaint;
#endif
}

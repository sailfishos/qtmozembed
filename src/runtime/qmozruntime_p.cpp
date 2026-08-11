/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "qmozruntime_p.h"

#include <QGuiApplication>
#include <qpa/qplatformnativeinterface.h>

#include "backendapi_p.h"
#include "qmessagepump.h"
#include "qmozembedlog.h"

#include "mozilla/embedlite/EmbedInitGlue.h"
#include "mozilla/embedlite/EmbedLiteAPI.h"
#include "mozilla/embedlite/EmbedLiteApp.h"
#include "mozilla/embedlite/EmbedLiteMessagePump.h"

using namespace mozilla::embedlite;

namespace {

void configureEGLDisplay(EmbedLiteApp *app)
{
    QPlatformNativeInterface * const nativeInterface =
            QGuiApplication::platformNativeInterface();
    void * const display = nativeInterface
            ? nativeInterface->nativeResourceForIntegration(
                  QByteArrayLiteral("egldisplay"))
            : nullptr;

    if (!display) {
        qCDebug(lcEmbedLiteExt) << "Qt did not provide an EGLDisplay;"
                                << "using Gecko's EGL display fallback";
    }
    app->SetEGLDisplay(display);

    // Keep the historical QtMozEmbed behaviour of always enabling accelerated
    // rendering.  The Qt display may not be available until the scene graph is
    // initialized, while Gecko can still create a working fallback display.
    app->SetIsAccelerated(true);
}

} // namespace

QMozRuntime::QMozRuntime(EmbedLiteAppListener *listener, bool asyncContext,
                         QObject *parent)
    : QObject(parent)
    , mApp(nullptr)
    , mQtPump(nullptr)
    , mEmbedStarted(false)
    , mAsyncContext(asyncContext)
{
    const QtMoz::BackendApiValidation validation =
            QtMoz::validateBackendApi(&QtMoz::embedLiteBackendApiV1());
    Q_ASSERT_X(validation.isValid(), __PRETTY_FUNCTION__,
               "Invalid built-in EmbedLite backend descriptor");
    Q_ASSERT_X(LoadEmbedLite(), __PRETTY_FUNCTION__, "Failed load XPCOMGlue");

    mApp = XRE_GetEmbedLite();
    mApp->SetListener(listener);
    if (mAsyncContext) {
        mQtPump = new MessagePumpQt(mApp);
    }
}

QMozRuntime::~QMozRuntime()
{
}

EmbedLiteApp *QMozRuntime::embedLiteApp() const
{
    return mApp;
}

EmbedLiteMessagePump *QMozRuntime::embedLoop() const
{
    return mQtPump ? mQtPump->EmbedLoop() : nullptr;
}

bool QMozRuntime::hasApp() const
{
    return mApp != nullptr;
}

bool QMozRuntime::isAsync() const
{
    return mAsyncContext;
}

void QMozRuntime::start()
{
    if (mEmbedStarted || !mApp) {
        return;
    }

    configureEGLDisplay(mApp);
    mEmbedStarted = true;
    if (mAsyncContext) {
        mApp->StartWithCustomPump(EmbedLiteApp::EMBED_THREAD,
                                  mQtPump->EmbedLoop());
    } else {
        mApp->Start(EmbedLiteApp::EMBED_THREAD);
        mEmbedStarted = false;
    }
}

void QMozRuntime::stop()
{
    if (mApp) {
        mApp->Stop();
    }
}

void QMozRuntime::detachListener()
{
    if (mApp) {
        mApp->SetListener(nullptr);
    }
}

void QMozRuntime::backendDestroyed()
{
    if (mQtPump) {
        mQtPump->deleteLater();
        mQtPump = nullptr;
    }
}

const QtMozBackendApiV1 &QMozRuntime::backendApi() const
{
    return QtMoz::embedLiteBackendApiV1();
}

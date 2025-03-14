/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "geckoworker.h"
#include "nsDebug.h"
#include "mozilla/embedlite/EmbedLiteApp.h"

using namespace mozilla::embedlite;

GeckoWorker::GeckoWorker(EmbedLiteApp *aApp, QObject *parent)
    : QObject(parent),
      mApp(aApp)
{
}

void GeckoWorker::doWork()
{
    if (mApp) {
        mApp->StartChildThread();
    }
}

void GeckoWorker::quit()
{
    printf("Call EmbedLiteApp::StopChildThread()\n");
    mApp->StopChildThread();
    deleteLater();
    sender()->deleteLater();
    mApp = nullptr;
}

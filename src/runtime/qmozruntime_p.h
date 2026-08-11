/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef QMOZRUNTIME_P_H
#define QMOZRUNTIME_P_H

#include <QObject>

#include "backendabi.h"

class MessagePumpQt;

namespace mozilla {
namespace embedlite {
class EmbedLiteApp;
class EmbedLiteAppListener;
class EmbedLiteMessagePump;
}
}

class Q_DECL_HIDDEN QMozRuntime : public QObject
{
public:
    explicit QMozRuntime(
            mozilla::embedlite::EmbedLiteAppListener *listener,
            bool asyncContext,
            QObject *parent = 0);
    ~QMozRuntime();

    mozilla::embedlite::EmbedLiteApp *embedLiteApp() const;
    mozilla::embedlite::EmbedLiteMessagePump *embedLoop() const;
    bool hasApp() const;
    bool isAsync() const;

    void start();
    void stop();
    void detachListener();
    void backendDestroyed();

    const QtMozBackendApiV1 &backendApi() const;

private:
    mozilla::embedlite::EmbedLiteApp *mApp;
    MessagePumpQt *mQtPump;
    bool mEmbedStarted;
    bool mAsyncContext;
};

#endif // QMOZRUNTIME_P_H

/****************************************************************************
**
** Copyright (C) 2016 Jolla Ltd.
** Contact: Raine Makelainen <raine.makelainen@jolla.com>
**
****************************************************************************/

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef QMOZCONTEXT_P_H
#define QMOZCONTEXT_P_H

#include <QObject>
#include <QMap>
#include <QStringList>
#include <QVariant>

#ifndef Q_MOC_RUN
#include "mozilla/embedlite/EmbedLiteApp.h"
#endif

class QMozRuntime;

namespace mozilla {
namespace embedlite {
class EmbedLiteApp;
class EmbedLiteAppListener;
class EmbedLiteMessagePump;
}
}

using namespace mozilla::embedlite;

class QMozContextPrivate : public QObject
#ifndef Q_MOC_RUN
    , public EmbedLiteAppListener
#endif
{
    Q_OBJECT
public:
    static QMozContextPrivate *instance();

    explicit QMozContextPrivate(QObject *parent = 0);
    ~QMozContextPrivate();

    void Initialized() override;
    void Destroyed() override;
    void OnObserve(const char *aTopic, const char16_t *aData) override;
    void LastWindowDestroyed() override;

    bool IsInitialized();
    EmbedLiteMessagePump *EmbedLoop();

Q_SIGNALS:
    void initialized();
    void contextDestroyed();
    void lastWindowDestroyed();
    void recvObserve(const QString message, const QVariant data);

private:
    QMozRuntime *mRuntime;
    std::map<std::string, uint> mObservers;

    bool mInitialized;
    QMap<QString, QVariant> mInitialPreferences;

    friend class QMozContext;
};

#endif // QMOZCONTEXT_P_H

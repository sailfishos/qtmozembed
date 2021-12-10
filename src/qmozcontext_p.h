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
#include <QThread>
#include <QVariant>

#include "qmozwindow.h"
#include "mozilla/embedlite/EmbedLiteApp.h"

class QMozViewCreator;
class MessagePumpQt;

namespace mozilla {
namespace embedlite {
class EmbedLiteView;
class EmbedLiteMessagePump;
}
}

using namespace mozilla::embedlite;

class QMozContextPrivate : public QObject, public EmbedLiteAppListener
{
    Q_OBJECT
public:
    static QMozContextPrivate *instance();

    explicit QMozContextPrivate(QObject *parent = 0);
    ~QMozContextPrivate();

    virtual bool ExecuteChildThread() override;
    virtual bool StopChildThread() override;
    virtual void Initialized() override;
    virtual void Destroyed() override;
    virtual void OnObserve(const char *aTopic, const char16_t *aData) override;
    virtual void LastViewDestroyed() override;
    virtual void LastWindowDestroyed() override;
    virtual uint32_t CreateNewWindowRequested(const uint32_t &chromeFlags,
                                              EmbedLiteView *aParentView,
                                              const uintptr_t &parentBrowsingContext) override;

    bool IsInitialized();
    EmbedLiteMessagePump *EmbedLoop();
    void destroyWindow();

Q_SIGNALS:
    void initialized();
    void contextDestroyed();
    void lastViewDestroyed();
    void lastWindowDestroyed();
    void recvObserve(const QString message, const QVariant data);

private:
    EmbedLiteApp *mApp;
    std::map<std::string, uint> mObservers;

    bool mInitialized;
    QPointer<QThread> mThread;
    bool mEmbedStarted;
    EmbedLiteMessagePump *mEventLoopPrivate;
    MessagePumpQt *mQtPump;
    bool mAsyncContext;
    QMozViewCreator *mViewCreator;
    QPointer<QMozWindow> mMozWindow;
    QMap<QString, QVariant> mInitialPreferences;

    friend class QMozContext;
};

#endif // QMOZCONTEXT_P_H

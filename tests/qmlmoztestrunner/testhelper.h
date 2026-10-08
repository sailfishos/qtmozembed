/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (c) 2021 Open Mobile Platform LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TEST_HELPER_H
#define TEST_HELPER_H

#include <QObject>
#include <QByteArray>
#include <QUrl>
#include <QVariantMap>
#include <QList>

class QTcpServer;

class TestHelper : public QObject
{
    Q_OBJECT

public:
    explicit TestHelper(QObject *parent = nullptr);

    Q_INVOKABLE QUrl serveFile(const QString &fileName);
    Q_INVOKABLE int requestCount() const { return mRequests.size(); }
    Q_INVOKABLE QString requestHeader(int index, const QString &name) const;
    Q_INVOKABLE QUrl redirectUrl() const;
    Q_INVOKABLE QString getenv(const QString &envVarName) const;
private:
    QTcpServer *mServer;
    QByteArray mContent;
    QByteArray mContentType;
    QList<QVariantMap> mRequests;
};

#endif

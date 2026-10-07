/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (c) 2021 Open Mobile Platform LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "testhelper.h"
#include <QString>
#include <QFile>
#include <QMimeDatabase>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>

TestHelper::TestHelper(QObject *parent)
    : QObject(parent)
    , mServer(new QTcpServer(this))
{
    connect(mServer, &QTcpServer::newConnection, this, [this]() {
        while (mServer->hasPendingConnections()) {
            QTcpSocket *socket = mServer->nextPendingConnection();
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
                QByteArray request = socket->property("request").toByteArray() + socket->readAll();
                if (request.size() > 8192) {
                    socket->disconnectFromHost();
                    return;
                }
                if (!request.contains("\r\n\r\n")) {
                    socket->setProperty("request", request);
                    return;
                }
                QVariantMap headers;
                const QList<QByteArray> lines = request.split('\n');
                for (const QByteArray &line : lines) {
                    const int colon = line.indexOf(':');
                    if (colon > 0)
                        headers.insert(QString::fromLatin1(line.left(colon).trimmed().toLower()),
                                       QString::fromLatin1(line.mid(colon + 1).trimmed()));
                }
                mRequests.append(headers);
                const QByteArray target = request.split(' ').value(1).split('?').value(0);
                if (request.startsWith("GET ") && target == "/redirect") {
                    socket->write("HTTP/1.1 302 Found\r\nLocation: /fixture\r\nCache-Control: no-store\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                } else if (!request.startsWith("GET ") || target != "/fixture") {
                    socket->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                } else {
                    socket->write("HTTP/1.1 200 OK\r\nContent-Type: " + mContentType
                                  + "\r\nContent-Length: "
                                  + QByteArray::number(mContent.size())
                                  + "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n" + mContent);
                }
                socket->disconnectFromHost();
            });
        }
    });
}

QUrl TestHelper::serveFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        return QUrl();
    }
    mContent = file.readAll();
    mContentType = QMimeDatabase().mimeTypeForFile(fileName).name().toLatin1();
    if (!mServer->isListening() && !mServer->listen(QHostAddress::LocalHost)) {
        return QUrl();
    }
    return QUrl(QStringLiteral("http://127.0.0.1:%1/fixture").arg(mServer->serverPort()));
}

QString TestHelper::getenv(const QString &envVarName) const
{
    return QString(::getenv(envVarName.toUtf8().constData()));
}

QString TestHelper::requestHeader(int index, const QString &name) const
{
    return mRequests.value(index).value(name.toLower()).toString();
}

QUrl TestHelper::redirectUrl() const
{
    return QUrl(QStringLiteral("http://127.0.0.1:%1/redirect").arg(mServer->serverPort()));
}

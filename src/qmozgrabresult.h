/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef QMOZGRABRESULT_H
#define QMOZGRABRESULT_H

#include <QtCore/QObject>
#include <QtCore/QSize>
#include <QtGui/QImage>

class QMozOpenGLWebPage;
class QMozGrabResultPrivate;

class QMozGrabResult : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QMozGrabResult)

    Q_PROPERTY(QImage image READ image CONSTANT)
public:
    ~QMozGrabResult();

    QImage image() const;
    bool isReady() const;

    Q_INVOKABLE bool saveToFile(const QString &fileName);

protected:
    bool event(QEvent *e);

Q_SIGNALS:
    void ready();

private:
    friend class QMozOpenGLWebPage;

    QMozGrabResult(QObject *parent = 0);
    void captureImage(const QRect &rect);

    QMozGrabResultPrivate *d_ptr;

    Q_DISABLE_COPY(QMozGrabResult)
};

QT_END_NAMESPACE

#endif

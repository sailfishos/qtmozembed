/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (c) 2015 - 2019 Jolla Ltd.
 * Copyright (c) 2019 Open Mobile Platform LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "qmozembedlog.h"
#include "qmozopenglwebpage.h"
#include "qmozgrabresult.h"
#include "qmozwindow.h"

#include <QCoreApplication>
#if defined(QT_OPENGL_ES_2)
#include <QOpenGLFunctions_ES2>
#else
#include <QOpenGLFunctions>
#endif
#include <QPointer>
#include <QSharedPointer>
#include <QWindow>

// Handle web page grab readiness through event loop so that connection type to the ready signal doesn't matter.
const QEvent::Type Event_WebPageGrab_Completed = static_cast<QEvent::Type>(QEvent::registerEventType());

QImage gl_read_framebuffer(const QRect &rect)
{
    QSize size = rect.size();
    int x = rect.x();
    int y = rect.y();

    while (glGetError());

    QImage img(size, QImage::Format_RGB32);
    GLint fmt = GL_BGRA_EXT;
    glReadPixels(x, y, size.width(), size.height(), fmt, GL_UNSIGNED_BYTE, img.bits());
    if (!glGetError())
        return img.mirrored();

    QImage rgbaImage(size, QImage::Format_RGBX8888);
    glReadPixels(x, y, size.width(), size.height(), GL_RGBA, GL_UNSIGNED_BYTE, rgbaImage.bits());
    if (!glGetError())
        return rgbaImage.mirrored();
    return QImage();
}

class QMozGrabResultPrivate
{
public:
    QMozGrabResultPrivate(QMozGrabResult *q)
        : q_ptr(q)
        , ready(false)
    {
    }

    static QMozGrabResult *create(QMozOpenGLWebPage *webPage, const QSize &targetSize);

    QMozGrabResult *q_ptr;
    QPointer<QMozOpenGLWebPage> webPage;
    QSize textureSize;
    QImage image;
    Qt::ScreenOrientation orientation;
    Qt::ScreenOrientation primaryOrientation;
    bool ready;
};

QMozGrabResult::~QMozGrabResult()
{
    delete d_ptr;
    d_ptr = 0;
}

/*!
 * \qmlproperty variant QMozGrabResult::image
 *
 * This property holds the pixel results from a grab in the
 * form of a QImage.
 */
QImage QMozGrabResult::image() const
{
    Q_D(const QMozGrabResult);
    return d->image;
}

bool QMozGrabResult::isReady() const
{
    Q_D(const QMozGrabResult);
    return d->ready;
}

bool QMozGrabResult::saveToFile(const QString &fileName)
{
    Q_D(QMozGrabResult);
    return d->image.save(fileName);
}

bool QMozGrabResult::event(QEvent *e)
{
    Q_D(QMozGrabResult);
    if (e->type() == Event_WebPageGrab_Completed) {
        d->ready = true;
        Q_EMIT ready();
        return true;
    }
    return QObject::event(e);
}

void QMozGrabResult::captureImage(const QRect &rect)
{
    Q_D(QMozGrabResult);
    int w = d->textureSize.width();
    int h = d->textureSize.height();

    if (d->primaryOrientation == Qt::PortraitOrientation
            && (d->orientation == Qt::LandscapeOrientation || d->orientation == Qt::InvertedLandscapeOrientation)) {
        qSwap<int>(w, h);
    }

    if (w > rect.width() || h > rect.height()) {
        // If the requested size is too large, shrink it to fit while retaining the aspect ratio.
        QSize size = QSize(w, h).scaled(rect.size(), Qt::KeepAspectRatio);
        w = size.width();
        h = size.height();
    }

    int x = d->orientation == Qt::LandscapeOrientation ? rect.width() - w : 0;
    int y = (d->orientation == Qt::PortraitOrientation
             || d->orientation == Qt::LandscapeOrientation) ? rect.height() - h : 0;

    QRect targetRect(x, y, w, h);
    QImage image = gl_read_framebuffer(targetRect);
    if (d->primaryOrientation == Qt::PortraitOrientation) {
        if (d->orientation != Qt::PortraitOrientation && d->orientation != Qt::PrimaryOrientation) {
            QMatrix rotationMatrix;
            switch (d->orientation) {
            case Qt::LandscapeOrientation:
                rotationMatrix.rotate(270);
                break;
            case Qt::InvertedLandscapeOrientation:
                rotationMatrix.rotate(90);
                break;
            case Qt::InvertedPortraitOrientation:
                rotationMatrix.rotate(180);
            default:
                break;
            }
            image = image.transformed(rotationMatrix);
        }
    } else if (d->orientation != Qt::LandscapeOrientation && d->orientation != Qt::PrimaryOrientation) {
        QMatrix rotationMatrix;
        switch (d->orientation) {
        case Qt::PortraitOrientation:
            rotationMatrix.rotate(90);
            break;
        case Qt::InvertedLandscapeOrientation:
            rotationMatrix.rotate(180);
            break;
        case Qt::InvertedPortraitOrientation:
            rotationMatrix.rotate(270);
        default:
            break;
        }
        image = image.transformed(rotationMatrix);
    }

    d->image = image;
    QCoreApplication::postEvent(this, new QEvent(Event_WebPageGrab_Completed));
}

QMozGrabResult::QMozGrabResult(QObject *parent)
    : QObject(parent)
    , d_ptr(new QMozGrabResultPrivate(this))
{
}

QMozGrabResult *QMozGrabResultPrivate::create(QMozOpenGLWebPage *webPage, const QSize &targetSize)
{
    Q_ASSERT(webPage);

    QSize size = targetSize;
    if (size.isEmpty()) {
        size = webPage->mozWindow()->size();
    }

    if (!size.isValid()) {
        qWarning() << "OpenGLWebPage::grabToImage web page has invalid dimensions";
        return 0;
    }

    if (!webPage->mozWindow()) {
        qWarning() << "OpenGLWebPage::grabToImage web page is not attached to a window";
        return 0;
    }

    if (!webPage->completed()) {
        qWarning() << "OpenGLWebPage::grabToImage web page is not yet completed. Implies that view is not created.";
        return 0;
    }

    if (!webPage->active()) {
        qWarning() << "OpenGLWebPage::grabToImage only active web page can be grabbed";
        return 0;
    }

    QMozGrabResult *result = new QMozGrabResult();
    QMozGrabResultPrivate *d = result->d_func();
    d->textureSize = size;
    d->webPage = webPage;
    d->orientation = webPage->mozWindow()->contentOrientation();
    d->primaryOrientation = webPage->mozWindow()->primaryOrientation();

    return result;
}

/*!
 * Grabs the web page into an in-memory image.
 *
 * The grab happens asynchronously and the signal QMozGrabResult::ready() is emitted
 * when the grab has been completed.
 *
 * The \a targetSize can be used to specify the size of the target image. By default, the
 * result will have the same size as the web page.
 *
 * \note This function copies surface from the GPU's memory into CPU's memory, which can
 * be quite costly operation. Smaller the \a targetSize better the performance.
 */
QSharedPointer<QMozGrabResult> QMozOpenGLWebPage::grabToImage(const QSize &targetSize)
{
    QSharedPointer<QMozGrabResult> result(QMozGrabResultPrivate::create(this, targetSize));
    if (result) {
        QMutexLocker lock(&mGrabResultListLock);
        mGrabResultList.append(result.toWeakRef());
        update();
    }
    return result;
}


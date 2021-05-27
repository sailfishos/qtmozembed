/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (c) 2015 - 2019 Jolla Ltd.
 * Copyright (c) 2019 Open Mobile Platform LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef QOPENGLWEBPAGE_H
#define QOPENGLWEBPAGE_H

#include <qqml.h>
#include <QSizeF>

// Needed events, all of these renders to qevent.h includes.
#include <QMouseEvent>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QTouchEvent>
#include <QPointer>
#include <QMutex>

#include "qmozview_defined_wrapper.h"

class QMozViewPrivate;
class QMozGrabResult;
class QMozWindow;
class QMozSecurity;

class QOpenGLWebPage : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool privateMode READ privateMode WRITE setPrivateMode NOTIFY privateModeChanged FINAL)
    Q_PROPERTY(bool completed READ completed NOTIFY completedChanged FINAL)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged FINAL)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged FINAL)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged FINAL)
    Q_PROPERTY(bool throttlePainting READ throttlePainting WRITE setThrottlePainting NOTIFY throttlePaintingChanged FINAL)
    Q_PROPERTY(int virtualKeyboardHeight WRITE setVirtualKeyboardHeight READ virtualKeyboardHeight NOTIFY virtualKeyboardHeightChanged FINAL)

    Q_MOZ_VIEW_PRORERTIES

public:
    explicit QOpenGLWebPage(QObject *parent = nullptr);
    virtual ~QOpenGLWebPage();

    Q_MOZ_VIEW_PUBLIC_METHODS

    bool privateMode() const;
    void setPrivateMode(bool privateMode);

    bool completed() const;

    bool enabled() const;
    void setEnabled(bool enabled);

    bool active() const;
    void setActive(bool active);

    bool loaded() const;

    QMozWindow *mozWindow() const;
    void setMozWindow(QMozWindow *window);

    bool throttlePainting() const;
    void setThrottlePainting(bool);

    int virtualKeyboardHeight() const;
    void setVirtualKeyboardHeight(int);

    void initialize();

    bool event(QEvent *event);
    QVariant inputMethodQuery(Qt::InputMethodQuery property) const;
    void inputMethodEvent(QInputMethodEvent *event);
    void keyPressEvent(QKeyEvent *);
    void keyReleaseEvent(QKeyEvent *);
    void focusInEvent(QFocusEvent *);
    void focusOutEvent(QFocusEvent *);
    void touchEvent(QTouchEvent *);
    void timerEvent(QTimerEvent *);

    QSharedPointer<QMozGrabResult> grabToImage(const QSize &targetSize = QSize());

public Q_SLOTS:
    Q_MOZ_VIEW_PUBLIC_SLOTS
    void update();
    void forceActiveFocus();
    void setInputMethodHints(Qt::InputMethodHints hints);

Q_SIGNALS:
    Q_MOZ_VIEW_SIGNALS
    void privateModeChanged();
    void completedChanged();
    void enabledChanged();
    void activeChanged();
    void widthChanged();
    void heightChanged();
    void loadedChanged();
    void throttlePaintingChanged();
    void virtualKeyboardHeightChanged();
    void afterRendering();

private Q_SLOTS:
    void processViewInitialization();
    void onDrawOverlay(const QRect &rect);

private:
    QMozViewPrivate *d;
    friend class QMozViewPrivate;

    bool mCompleted;
    QList<QWeakPointer<QMozGrabResult> > mGrabResultList;
    QMutex mGrabResultListLock;
    bool mSizeUpdateScheduled;
    bool mThrottlePainting;
    int m_virtualKeyboardHeight;

    Q_DISABLE_COPY(QOpenGLWebPage)
};

QML_DECLARE_TYPE(QOpenGLWebPage)

#endif // QOPENGLWEBPAGE_H

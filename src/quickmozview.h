/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (c) 2013 - 2019 Jolla Ltd.
 * Copyright (c) 2019 Open Mobile Platform LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef QuickMozView_H
#define QuickMozView_H

#include <QMatrix>
#include <QMutex>
#include <QtQuick/QQuickItem>
#include <QtGui/QOpenGLShaderProgram>
#include "qmozview_defined_wrapper.h"

QT_BEGIN_NAMESPACE
class QSGTexture;
QT_END_NAMESPACE

class QMozViewPrivate;
class QMozWindow;
class QMozSecurity;

class QuickMozView : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(bool privateMode READ privateMode WRITE setPrivateMode NOTIFY privateModeChanged FINAL)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged FINAL)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged FINAL)
    Q_PROPERTY(QObject *child READ getChild NOTIFY childChanged)
    Q_PROPERTY(Qt::ScreenOrientation orientation READ orientation WRITE setOrientation NOTIFY orientationChanged RESET resetOrientation FINAL)
    Q_PROPERTY(qreal viewportWidth READ viewportWidth WRITE setViewportWidth NOTIFY viewportWidthChanged RESET resetViewportWidth)
    Q_PROPERTY(qreal viewportHeight READ viewportHeight WRITE setViewportHeight NOTIFY viewportHeightChanged RESET resetViewportHeight)

    Q_MOZ_VIEW_PRORERTIES

public:
    QuickMozView(QQuickItem *parent = 0);
    ~QuickMozView();

    Q_MOZ_VIEW_PUBLIC_METHODS
    void RenderToCurrentContext();

    bool privateMode() const;

    bool active() const;
    void setActive(bool active);
    void setPrivateMode(bool);

    bool loaded() const;

    Qt::ScreenOrientation orientation() const;
    void setOrientation(Qt::ScreenOrientation orientation);
    void resetOrientation();

    qreal viewportWidth() const;
    void setViewportWidth(qreal width);
    void resetViewportWidth();

    qreal viewportHeight() const;
    void setViewportHeight(qreal height);
    void resetViewportHeight();

private:
    QObject *getChild()
    {
        return this;
    }
    void updateGLContextInfo();

public Q_SLOTS:
    Q_MOZ_VIEW_PUBLIC_SLOTS

Q_SIGNALS:
    void childChanged();
    void setIsActive(bool);
    void privateModeChanged();
    void activeChanged();
    void loadedChanged();
    void followItemGeometryChanged();
    void orientationChanged();
    void viewportWidthChanged();
    void viewportHeightChanged();

    Q_MOZ_VIEW_SIGNALS

private Q_SLOTS:
    void processViewInitialization();
    void SetIsActive(bool aIsActive);
    void resumeRendering();
    void compositingFinished();
    void updateMargins();

// INTERNAL
protected:
    void itemChange(ItemChange change, const ItemChangeData &) override;
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *data) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery property) const override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    void keyPressEvent(QKeyEvent *) override;
    void keyReleaseEvent(QKeyEvent *) override;
    void focusInEvent(QFocusEvent *) override;
    void focusOutEvent(QFocusEvent *) override;
    void touchEvent(QTouchEvent *) override;
    void timerEvent(QTimerEvent *) override;
    void componentComplete() override;
    void releaseResources() override;

    void updatePolish() override;

public Q_SLOTS:
    void setInputMethodHints(Qt::InputMethodHints hints);

private Q_SLOTS:
    void updateEnabled();
    void updateOrientation(Qt::ScreenOrientation orientation);

private:
    void updateContentSize(const QSizeF &size);
    void prepareMozWindow();

    QMozViewPrivate *d;
    QSGTexture *mTexture;
    friend class QMozViewPrivate;
    Qt::ScreenOrientation mOrientation;
    bool mExplicitViewportWidth;
    bool mExplicitViewportHeight;
    bool mExplicitOrientation;
    bool mUseQmlMouse;
    bool mComposited;
    bool mFollowItemGeometry;
};

#endif // QuickMozView_H

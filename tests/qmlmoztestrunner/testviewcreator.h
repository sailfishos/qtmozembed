/****************************************************************************
**
** Copyright (c) 2014 - 2021 Jolla Ltd.
** Copyright (c) 2021 Open Mobile Platform LLC.
**
****************************************************************************/

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef testviewcreator_h
#define testviewcreator_h

#include "qmozviewcreator.h"
#include "quickmozview.h"

#include <QPointer>

class TestViewCreator : public QMozViewCreator
{
    Q_OBJECT
    Q_PROPERTY(QQuickItem *parentItem READ parentItem WRITE setParentItem NOTIFY parentItemChanged)
    Q_PROPERTY(QQmlComponent *webViewComponent READ webViewComponent WRITE setWebViewComponent NOTIFY webViewComponentChanged)

public:
    explicit TestViewCreator(QObject *parent = 0);
    ~TestViewCreator() {}

    QQuickItem *parentItem() const;
    void setParentItem(QQuickItem *parentItem);

    QQmlComponent *webViewComponent() const;
    void setWebViewComponent(QQmlComponent *webViewComponent);

    virtual quint32 createView(const quint32 &parentId, const uintptr_t &parentBrowsingContext) override;

signals:
    void aboutToCreateNewView();
    void newViewCreated(QuickMozView *view);
    void webViewComponentChanged();
    void parentItemChanged();

private:
    QPointer<QQuickItem> m_parentItem;
    QPointer<QQmlComponent> m_webViewComponent;
};

#endif

/****************************************************************************
**
** Copyright (c) 2014 - 2021 Jolla Ltd.
** Copyright (c) 2021 Open Mobile Platform LLC.
**
****************************************************************************/

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "qmozcontext.h"
#include "testviewcreator.h"

#include <QQmlContext>
#include <QQmlEngine>
#include <qqmlinfo.h>

TestViewCreator::TestViewCreator(QObject *parent)
    : QMozViewCreator(parent)
{
    QMozContext::instance()->setViewCreator(this);
}

QQuickItem *TestViewCreator::parentItem() const
{
    return m_parentItem;
}

void TestViewCreator::setParentItem(QQuickItem *parentItem)
{
    if (m_parentItem != parentItem) {
        m_parentItem = parentItem;
        emit parentItemChanged();
    }
}

QQmlComponent *TestViewCreator::webViewComponent() const
{
    return m_webViewComponent;
}

void TestViewCreator::setWebViewComponent(QQmlComponent *webViewComponent)
{
    if (m_webViewComponent != webViewComponent) {
        m_webViewComponent = webViewComponent;
        emit webViewComponentChanged();
    }
};

quint32 TestViewCreator::createView(const quint32 &parentId, const uintptr_t &parentBrowsingContext)
{
    emit aboutToCreateNewView();

    QQmlContext *context = QQmlEngine::contextForObject(this);
    quint32 uniqueId = 0;
    QObject *object = m_webViewComponent->beginCreate(context);
    if (object) {
        Q_ASSERT_X(m_parentItem, __PRETTY_FUNCTION__, "There must be parentItem set.");
        object->setParent(m_parentItem);
        QuickMozView* webView = qobject_cast<QuickMozView *>(object);
        if (webView) {
            webView->setParentId(parentId);
            webView->setParentItem(m_parentItem);
            webView->setParentBrowsingContext(parentBrowsingContext);
            QQmlEngine::setObjectOwnership(webView, QQmlEngine::JavaScriptOwnership);
            m_webViewComponent->completeCreate();
            uniqueId = webView->uniqueId();
            emit newViewCreated(webView);
        } else {
            qmlInfo(this) << "webViewComponent must be a QmlMozView component";
            m_webViewComponent->completeCreate();
            delete object;
            emit newViewCreated(nullptr);
        }
    } else {
        qmlInfo(this) << "Creation of the webView failed. Error: " << m_webViewComponent->errorString();
        delete context;
        emit newViewCreated(nullptr);
    }


    return uniqueId;
};

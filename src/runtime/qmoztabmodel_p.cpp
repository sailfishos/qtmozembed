/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "qmoztabmodel_p.h"

QMozTabModel::QMozTabModel(QObject *parent)
    : QAbstractListModel(parent)
    , mSelectedTabId(0)
    , mSelectedTabIndex(-1)
    , mRevision(0)
{
}

int QMozTabModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : mTabs.count();
}

int QMozTabModel::count() const
{
    return mTabs.count();
}

QVariant QMozTabModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= mTabs.count()) {
        return QVariant();
    }

    const QMozChromeTabSnapshot &tab = mTabs.at(index.row());
    switch (role) {
    case IdRole:
        return QString::number(tab.id);
    case OpenerIdRole:
        return tab.openerId ? QString::number(tab.openerId) : QString();
    case PersistentIdRole:
        return QString::number(tab.persistentId);
    case LocationRevisionRole:
        return QString::number(tab.locationRevision);
    case LocationRole:
        return tab.location;
    case TitleRole:
        return tab.title;
    case LoadingRole:
        return tab.loading;
    case ClosingRole:
        return tab.closing;
    case DiscardedRole:
        return tab.discarded;
    case CanGoBackRole:
        return tab.canGoBack;
    case CanGoForwardRole:
        return tab.canGoForward;
    case ProgressRole:
        return tab.progress;
    case CurrentRole:
        return tab.current;
    case TotalRole:
        return tab.total;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> QMozTabModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(IdRole, "tabId");
    roles.insert(OpenerIdRole, "openerId");
    roles.insert(PersistentIdRole, "persistentId");
    roles.insert(LocationRevisionRole, "locationRevision");
    roles.insert(LocationRole, "location");
    roles.insert(TitleRole, "title");
    roles.insert(LoadingRole, "loading");
    roles.insert(ClosingRole, "closing");
    roles.insert(DiscardedRole, "discarded");
    roles.insert(CanGoBackRole, "canGoBack");
    roles.insert(CanGoForwardRole, "canGoForward");
    roles.insert(ProgressRole, "progress");
    roles.insert(CurrentRole, "current");
    roles.insert(TotalRole, "total");
    return roles;
}

QVariantMap QMozTabModel::rowData(
        const QMozChromeTabSnapshot &tab) const
{
    QVariantMap row;
    row.insert(QStringLiteral("tabId"), QString::number(tab.id));
    row.insert(QStringLiteral("openerId"),
               tab.openerId ? QString::number(tab.openerId) : QString());
    row.insert(QStringLiteral("persistentId"),
               QString::number(tab.persistentId));
    row.insert(QStringLiteral("locationRevision"),
               QString::number(tab.locationRevision));
    row.insert(QStringLiteral("location"), tab.location);
    row.insert(QStringLiteral("title"), tab.title);
    row.insert(QStringLiteral("loading"), tab.loading);
    row.insert(QStringLiteral("closing"), tab.closing);
    row.insert(QStringLiteral("discarded"), tab.discarded);
    row.insert(QStringLiteral("canGoBack"), tab.canGoBack);
    row.insert(QStringLiteral("canGoForward"), tab.canGoForward);
    row.insert(QStringLiteral("progress"), tab.progress);
    row.insert(QStringLiteral("current"), tab.current);
    row.insert(QStringLiteral("total"), tab.total);
    return row;
}

QVariantList QMozTabModel::snapshot() const
{
    QVariantList result;
    result.reserve(mTabs.count());
    for (const QMozChromeTabSnapshot &tab : mTabs) {
        result.append(rowData(tab));
    }
    return result;
}

void QMozTabModel::setSnapshot(
        const QVector<QMozChromeTabSnapshot> &tabs, quint64 selectedTabId,
        quint64 revision)
{
    const int oldCount = mTabs.count();
    const quint64 oldRevision = mRevision;
    const QString oldSelectedTabOpenerId = selectedTabOpenerId();
    beginResetModel();
    mTabs = tabs;
    mSelectedTabId = selectedTabId;
    mRevision = revision;
    mSelectedTabIndex = -1;
    for (int i = 0; i < mTabs.count(); ++i) {
        if (mTabs.at(i).id == selectedTabId) {
            mSelectedTabIndex = i;
            break;
        }
    }
    endResetModel();
    if (oldCount != mTabs.count()) {
        Q_EMIT countChanged();
    }
    if (oldRevision != mRevision) {
        Q_EMIT revisionChanged();
    }
    if (oldSelectedTabOpenerId != selectedTabOpenerId()) {
        Q_EMIT selectedTabOpenerIdChanged();
    }
}

void QMozTabModel::clear()
{
    if (mTabs.isEmpty() && mSelectedTabId == 0 && mRevision == 0) {
        return;
    }
    setSnapshot(QVector<QMozChromeTabSnapshot>(), 0, 0);
}

QString QMozTabModel::selectedTabId() const
{
    return mSelectedTabId ? QString::number(mSelectedTabId) : QString();
}

QString QMozTabModel::selectedTabOpenerId() const
{
    const QMozChromeTabSnapshot * const tab = selectedTab();
    return tab && tab->openerId ? QString::number(tab->openerId) : QString();
}

int QMozTabModel::selectedTabIndex() const
{
    return mSelectedTabIndex;
}

const QMozChromeTabSnapshot *QMozTabModel::selectedTab() const
{
    return mSelectedTabIndex >= 0 && mSelectedTabIndex < mTabs.count()
            ? &mTabs.at(mSelectedTabIndex) : nullptr;
}

QString QMozTabModel::revision() const
{
    return mRevision ? QString::number(mRevision) : QString();
}

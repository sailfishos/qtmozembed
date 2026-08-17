/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef QMOZTABMODEL_P_H
#define QMOZTABMODEL_P_H

#include "qmozchromesession_p.h"

#include <QAbstractListModel>
#include <QVariantList>

class Q_DECL_HIDDEN QMozTabModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    Q_PROPERTY(QString revision READ revision NOTIFY revisionChanged FINAL)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        PersistentIdRole,
        LocationRevisionRole,
        LocationRole,
        TitleRole,
        LoadingRole,
        ClosingRole,
        DiscardedRole,
        CanGoBackRole,
        CanGoForwardRole,
        ProgressRole,
        CurrentRole,
        TotalRole
    };

    explicit QMozTabModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int count() const;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE QVariantList snapshot() const;

    void setSnapshot(const QVector<QMozChromeTabSnapshot> &tabs,
                     quint64 selectedTabId, quint64 revision);
    void clear();

    QString selectedTabId() const;
    int selectedTabIndex() const;
    const QMozChromeTabSnapshot *selectedTab() const;
    QString revision() const;

Q_SIGNALS:
    void countChanged();
    void revisionChanged();

private:
    QVariantMap rowData(const QMozChromeTabSnapshot &tab) const;

    QVector<QMozChromeTabSnapshot> mTabs;
    quint64 mSelectedTabId;
    int mSelectedTabIndex;
    quint64 mRevision;
};

#endif // QMOZTABMODEL_P_H

/* Copyright (C) 2026 Jolla Mobile Ltd
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QMOZMEDIACONTROLLER_H
#define QMOZMEDIACONTROLLER_H

#include <QObject>
#include <QPointer>
#include <QElapsedTimer>
#include <QMap>
#include <QSet>
#include <QVariantMap>
#include "runtime/qmozchromesession_p.h"

// One application-wide projection of Gecko's main controller. All times exposed
// here are seconds; controller and tab IDs are strings to avoid JS precision loss.
class QMozMediaController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QObject *ownerView READ ownerView NOTIFY changed)
    Q_PROPERTY(QString ownerViewId READ ownerViewId NOTIFY changed)
    Q_PROPERTY(QString ownerTabId READ ownerTabId NOTIFY changed)
    Q_PROPERTY(bool privateBrowsing READ privateBrowsing NOTIFY changed)
    Q_PROPERTY(QString controllerId READ controllerId NOTIFY changed)
    Q_PROPERTY(QString trackId READ trackId NOTIFY changed)
    Q_PROPERTY(QString playbackState READ playbackState NOTIFY changed)
    Q_PROPERTY(QVariantMap metadata READ metadata NOTIFY changed)
    Q_PROPERTY(bool canPlay READ canPlay NOTIFY changed)
    Q_PROPERTY(bool canPause READ canPause NOTIFY changed)
    Q_PROPERTY(bool canPlayPause READ canPlayPause NOTIFY changed)
    Q_PROPERTY(bool canStop READ canStop NOTIFY changed)
    Q_PROPERTY(bool canGoNext READ canGoNext NOTIFY changed)
    Q_PROPERTY(bool canGoPrevious READ canGoPrevious NOTIFY changed)
    Q_PROPERTY(bool canSeek READ canSeek NOTIFY changed)
    Q_PROPERTY(bool hasPosition READ hasPosition NOTIFY changed)
    Q_PROPERTY(double duration READ duration NOTIFY changed)
    Q_PROPERTY(double position READ position NOTIFY changed)
public:
    explicit QMozMediaController(QObject *parent = nullptr);
    bool available() const;
    QObject *ownerView() const;
    QString ownerViewId() const;
    QString ownerTabId() const;
    bool privateBrowsing() const;
    QString controllerId() const;
    QString trackId() const;
    QString playbackState() const;
    QVariantMap metadata() const;
    bool canPlay() const { return supports(QMozMediaCommand::Play); }
    bool canPause() const { return supports(QMozMediaCommand::Pause); }
    bool canPlayPause() const { return supports(QMozMediaCommand::PlayPause); }
    bool canStop() const { return supports(QMozMediaCommand::Stop); }
    bool canGoNext() const { return supports(QMozMediaCommand::Next); }
    bool canGoPrevious() const { return supports(QMozMediaCommand::Previous); }
    bool canSeek() const { return hasPosition() && supports(QMozMediaCommand::Seek); }
    bool hasPosition() const;
    double duration() const;
    double position() const;
    Q_INVOKABLE bool play();
    Q_INVOKABLE bool pause();
    Q_INVOKABLE bool playPause();
    Q_INVOKABLE bool stop();
    Q_INVOKABLE bool next();
    Q_INVOKABLE bool previous();
    Q_INVOKABLE bool seek(const QString &trackId, double seconds);
    using CommandHandler = std::function<bool(quint64, quint64, quint64, QMozMediaCommand, double)>;
    void update(QObject *owner, quint32 viewId, const QMozMediaState &state,
                const CommandHandler &handler);
    void retainTabs(QObject *owner, const QVector<QMozChromeTabSnapshot> &tabs);
    void removeOwner(QObject *owner);
    void clear();
Q_SIGNALS:
    void changed();
    void seeked(double seconds);
private:
    struct Entry {
        QPointer<QObject> owner;
        quint32 viewId = 0;
        QMozMediaState state;
        QElapsedTimer clock;
        CommandHandler command;
    };
    bool supports(QMozMediaCommand command) const;
    bool send(QMozMediaCommand command, double seconds = 0);
    void select();
    QSet<QObject *> mOwners;
    QMap<QObject *, QMap<quint64, quint64>> mTabs;
    QMap<QString, Entry> mEntries;
    quint64 mMainControllerId = 0;
    Entry mCurrent;
};
#endif

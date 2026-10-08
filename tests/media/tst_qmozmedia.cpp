/* Copyright (C) 2026 Jolla Mobile Ltd
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "qmozmediacontroller_p.h"
#include <QtTest>
#include <limits>

class MediaTest : public QObject
{
    Q_OBJECT
private slots:
    void replacementAndCommands()
    {
        QMozMediaController media;
        QObject owner;
        QMozMediaState state;
        state.tabId = 9007199254740993ULL;
        state.controllerId = state.mainControllerId = 12;
        state.controllerToken = 100;
        state.trackToken = 101;
        state.active = state.playing = true;
        state.capabilities = (1 << quint8(QMozMediaCommand::Play))
            | (1 << quint8(QMozMediaCommand::Pause)) | (1 << quint8(QMozMediaCommand::Seek));
        state.hasPosition = true;
        state.position = 10.25;
        state.duration = 120;
        state.playbackRate = 1;
        double seek = -1;
        quint64 acceptedToken = 100;
        auto handler = [&](quint64 tab, quint64 token, quint64 track,
                           QMozMediaCommand command, double seconds) {
            if (token != acceptedToken) return false;
            if (tab != state.tabId) return false;
            if (command == QMozMediaCommand::Seek) {
                if (track != state.trackToken) return false;
                seek = seconds;
            }
            return true;
        };
        media.update(&owner, 5, state, handler);
        QCOMPARE(media.ownerTabId(), QStringLiteral("9007199254740993"));
        QVERIFY(media.canPlay());
        QVERIFY(media.canPause());
        QVERIFY(!media.canGoNext());
        QVERIFY(media.canSeek());
        const QString oldTrack = media.trackId();
        QVERIFY(media.seek(oldTrack, 12.125));
        QCOMPARE(seek, 12.125);
        QVERIFY(!media.seek(oldTrack, std::numeric_limits<double>::quiet_NaN()));
        QVERIFY(!media.seek(oldTrack, -1));
        QVERIFY(!media.seek(oldTrack, 121));
        acceptedToken = 200; // Native replacement precedes the asynchronous snapshot.
        QVERIFY(!media.play());
        state.controllerToken = 200;
        state.trackToken = 201;
        media.update(&owner, 5, state, handler);
        QVERIFY(!media.seek(oldTrack, 5));
        QVERIFY(media.play());
        media.removeOwner(&owner);
        QVERIFY(!media.available());
        QVERIFY(media.metadata().isEmpty());
        QVERIFY(!media.play());
    }
    void privacyAndIndependentTabs()
    {
        QMozMediaController media;
        QObject publicOwner, privateOwner;
        QMozMediaState a;
        a.tabId = 1; a.controllerId = a.mainControllerId = 10;
        a.controllerToken = 11; a.trackToken = 12;
        a.active = a.playing = true; a.title = "Public";
        media.update(&publicOwner, 1, a, {});
        QVERIFY(media.available()); QVERIFY(!media.privateBrowsing());
        QMozMediaState b = a;
        b.tabId = 2; b.controllerId = b.mainControllerId = 20;
        b.controllerToken = 21; b.trackToken = 22;
        b.privateBrowsing = true; b.title = "Private";
        // A selection notification must immediately clear the old public main.
        QMozMediaState selection; selection.mainControllerId = 20;
        media.update(&publicOwner, 1, selection, {});
        QVERIFY(!media.available());
        media.update(&privateOwner, 2, b, {});
        QVERIFY(media.available()); QVERIFY(media.privateBrowsing());
        a.mainControllerId = 20; // Public tab updates cannot steal selection.
        media.update(&publicOwner, 1, a, {});
        QVERIFY(media.privateBrowsing());
        b.playing = false;
        media.update(&privateOwner, 2, b, {});
        QCOMPARE(media.playbackState(), QStringLiteral("Paused"));
        a.mainControllerId = 10;
        media.update(&publicOwner, 1, a, {});
        QVERIFY(!media.privateBrowsing());
        QCOMPARE(media.metadata().value("title").toString(), QStringLiteral("Public"));
        selection.mainControllerId = 0;
        media.update(&publicOwner, 1, selection, {});
        QVERIFY(!media.available());
    }
    void navigationAndTeardown()
    {
        QMozMediaController media;
        QObject *owner = new QObject;
        QMozMediaState state;
        state.tabId = 1; state.locationRevision = 7;
        state.controllerId = state.mainControllerId = 10;
        state.controllerToken = 11; state.trackToken = 12; state.active = true;
        media.update(owner, 1, state, {});
        QVERIFY(media.available());
        QMozChromeTabSnapshot tab = {};
        tab.id = 1; tab.locationRevision = 8;
        media.retainTabs(owner, {tab});
        QVERIFY(!media.available());
        media.update(owner, 1, state, {}); // delayed old-document snapshot
        QVERIFY(!media.available());
        state.locationRevision = 8;
        media.update(owner, 1, state, {});
        QVERIFY(media.available());
        QSignalSpy changes(&media, &QMozMediaController::changed);
        delete owner;
        QVERIFY(!media.available());
        QCOMPARE(changes.count(), 1);
    }
};
QTEST_GUILESS_MAIN(MediaTest)
#include "tst_qmozmedia.moc"

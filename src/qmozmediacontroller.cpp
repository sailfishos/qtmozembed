/* Copyright (C) 2026 Jolla Mobile Ltd
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "qmozmediacontroller_p.h"
#include <algorithm>
#include <cmath>

QMozMediaController::QMozMediaController(QObject *parent) : QObject(parent) {}
bool QMozMediaController::available() const
{
    return mCurrent.owner && mCurrent.state.active && mCurrent.state.controllerToken;
}
QObject *QMozMediaController::ownerView() const { return available() ? mCurrent.owner.data() : nullptr; }
QString QMozMediaController::ownerViewId() const { return available() ? QString::number(mCurrent.viewId) : QString(); }
QString QMozMediaController::ownerTabId() const { return available() ? QString::number(mCurrent.state.tabId) : QString(); }
bool QMozMediaController::privateBrowsing() const { return available() && mCurrent.state.privateBrowsing; }
QString QMozMediaController::controllerId() const { return available() ? QString::number(mCurrent.state.controllerToken) : QString(); }
QString QMozMediaController::trackId() const
{
    return available() ? QStringLiteral("/org/mpris/MediaPlayer2/track/t%1").arg(mCurrent.state.trackToken) : QString();
}
QString QMozMediaController::playbackState() const
{
    return !available() ? QStringLiteral("Stopped") : mCurrent.state.playing
        ? QStringLiteral("Playing") : QStringLiteral("Paused");
}
QVariantMap QMozMediaController::metadata() const
{
    if (!available()) {
        return {};
    }
    return {{QStringLiteral("title"), mCurrent.state.title},
            {QStringLiteral("artist"), mCurrent.state.artist},
            {QStringLiteral("album"), mCurrent.state.album}};
}
bool QMozMediaController::supports(QMozMediaCommand command) const
{
    return available() && (mCurrent.state.capabilities & (1u << quint8(command)));
}
bool QMozMediaController::hasPosition() const
{
    return available() && mCurrent.state.hasPosition && std::isfinite(mCurrent.state.duration)
        && mCurrent.state.duration >= 0 && std::isfinite(mCurrent.state.position)
        && std::isfinite(mCurrent.state.playbackRate);
}
double QMozMediaController::duration() const { return hasPosition() ? mCurrent.state.duration : 0; }
double QMozMediaController::position() const
{
    if (!hasPosition()) {
        return 0;
    }
    const double elapsed = mCurrent.clock.isValid() ? mCurrent.clock.elapsed() / 1000.0 : 0;
    const double position = mCurrent.state.position + (mCurrent.state.playing
        ? elapsed * mCurrent.state.playbackRate : 0);
    return std::max(0.0, std::min(duration(), position));
}
bool QMozMediaController::send(QMozMediaCommand command, double seconds)
{
    // Copy before calling Gecko: its callbacks can synchronously replace state.
    const Entry current = mCurrent;
    return supports(command) && current.command && current.command(current.state.tabId,
            current.state.controllerToken, current.state.trackToken, command, seconds);
}
bool QMozMediaController::play() { return send(QMozMediaCommand::Play); }
bool QMozMediaController::pause() { return send(QMozMediaCommand::Pause); }
bool QMozMediaController::playPause() { return send(QMozMediaCommand::PlayPause); }
bool QMozMediaController::stop() { return send(QMozMediaCommand::Stop); }
bool QMozMediaController::next() { return send(QMozMediaCommand::Next); }
bool QMozMediaController::previous() { return send(QMozMediaCommand::Previous); }
bool QMozMediaController::seek(const QString &track, double seconds)
{
    return canSeek() && track == trackId() && std::isfinite(seconds)
        && seconds >= 0 && seconds <= duration() && send(QMozMediaCommand::Seek, seconds);
}
void QMozMediaController::select()
{
    mCurrent = Entry();
    for (const Entry &entry : mEntries) {
        if (entry.owner && entry.state.active && entry.state.controllerId == mMainControllerId) {
            mCurrent = entry;
            break;
        }
    }
    Q_EMIT changed();
}
void QMozMediaController::update(QObject *owner, quint32 viewId,
        const QMozMediaState &state, const CommandHandler &handler)
{
    if (!owner) {
        return;
    }
    if (!mOwners.contains(owner)) {
        mOwners.insert(owner);
        connect(owner, &QObject::destroyed, this, [this, owner]() { removeOwner(owner); });
    }
    if (state.tabId && mTabs.contains(owner)
            && (!mTabs.value(owner).contains(state.tabId)
                || mTabs.value(owner).value(state.tabId) != state.locationRevision)) {
        return;
    }
    const bool discontinuity = state.mainControllerId == mMainControllerId
            && state.controllerId == state.mainControllerId && available() && hasPosition() && state.hasPosition
            && state.active && state.controllerToken == mCurrent.state.controllerToken
            && state.trackToken == mCurrent.state.trackToken
            && std::abs(state.position - position()) > 1.0;
    mMainControllerId = state.mainControllerId;
    if (state.tabId) {
        const QString key = QStringLiteral("%1/%2").arg(viewId).arg(state.tabId);
        Entry entry;
        entry.owner = owner;
        entry.viewId = viewId;
        entry.state = state;
        entry.command = handler;
        entry.clock.start();
        mEntries.insert(key, entry);
    }
    select();
    if (discontinuity && available()) {
        Q_EMIT seeked(position());
    }
}
void QMozMediaController::retainTabs(QObject *owner, const QVector<QMozChromeTabSnapshot> &tabs)
{
    QMap<quint64, quint64> revisions;
    for (const auto &tab : tabs) {
        if (!tab.discarded) {
            revisions.insert(tab.id, tab.locationRevision);
        }
    }
    mTabs.insert(owner, revisions);
    for (auto it = mEntries.begin(); it != mEntries.end();) {
        bool found = it->owner != owner;
        for (const auto &tab : tabs) {
            if (tab.id == it->state.tabId && tab.locationRevision == it->state.locationRevision && !tab.discarded) {
                found = true;
            }
        }
        if (!found) {
            it = mEntries.erase(it);
        } else {
            ++it;
        }
    }
    select();
}
void QMozMediaController::removeOwner(QObject *owner)
{
    mOwners.remove(owner);
    mTabs.remove(owner);
    for (auto it = mEntries.begin(); it != mEntries.end();) {
        if (!it->owner || it->owner == owner) {
            it = mEntries.erase(it);
        } else {
            ++it;
        }
    }
    select();
}
void QMozMediaController::clear()
{
    mEntries.clear();
    mTabs.clear();
    mMainControllerId = 0;
    select();
}

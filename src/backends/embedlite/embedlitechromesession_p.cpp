/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "embedlitechromesession_p.h"

#include "embedlitesurface_p.h"

#include <QByteArray>
#include <QEnableSharedFromThis>
#include <QSet>
#include <QString>
#include <QVector>

#include <mozilla/embedlite/EmbedLiteChromeSession.h>
#include <mozilla/embedlite/EmbedLiteChromeContentSession.h>
#include <mozilla/embedlite/EmbedLiteChromeInputSession.h>
#include <mozilla/embedlite/EmbedLiteChromeTabSession.h>
#include <mozilla/embedlite/EmbedInputData.h>
#include <mozilla/embedlite/EmbedLiteWindow.h>

using namespace mozilla::embedlite;

namespace {

class EmbedLiteChromeSessionAdapter final
    : public QMozChromeSession
    , public EmbedLiteChromeSessionListener
    , public EmbedLiteChromeContentSessionListener
    , public EmbedLiteChromeInputSessionListener
    , public EmbedLiteChromeTabSessionListener
    , public QEnableSharedFromThis<EmbedLiteChromeSessionAdapter>
{
public:
    explicit EmbedLiteChromeSessionAdapter(
            EmbedLiteChromeSession *legacySession,
            EmbedLiteChromeTabSession *tabSession,
            EmbedLiteChromeInputSession *inputSession,
            EmbedLiteChromeContentSession *contentSession,
            quint32 uniqueId)
        : mLegacySession(legacySession)
        , mTabSession(tabSession)
        , mInputSession(inputSession)
        , mContentSession(contentSession)
        , mUniqueId(uniqueId)
    {
    }

    ~EmbedLiteChromeSessionAdapter() override
    {
        if (mLegacySession) {
            mLegacySession->SetListener(nullptr);
        }
        if (mTabSession) {
            mTabSession->SetTabListener(nullptr);
        }
        if (mInputSession) {
            mInputSession->SetInputListener(nullptr);
        }
        if (mContentSession) {
            mContentSession->SetContentListener(nullptr);
        }
    }

    quint32 uniqueId() const override
    {
        return mUniqueId;
    }

    void setCallbacks(
            const QMozChromeSessionCallbacks &callbacks) override
    {
        const QSharedPointer<EmbedLiteChromeSessionAdapter> self =
                sharedFromThis();
        if (self.isNull()) {
            return;
        }
        mCallbacks = callbacks;
        if (mLegacySession) {
            mLegacySession->SetListener(this);
        }
        if (mTabSession) {
            mTabSession->SetTabListener(this);
        }
        if (mInputSession) {
            mInputSession->SetInputListener(this);
        }
        if (mContentSession) {
            mContentSession->SetContentListener(this);
        }
    }

    void clearCallbacks() override
    {
        const QSharedPointer<EmbedLiteChromeSessionAdapter> self =
                sharedFromThis();
        if (self.isNull()) {
            return;
        }
        if (mLegacySession) {
            mLegacySession->SetListener(nullptr);
        }
        if (mTabSession) {
            mTabSession->SetTabListener(nullptr);
        }
        if (mInputSession) {
            mInputSession->SetInputListener(nullptr);
        }
        if (mContentSession) {
            mContentSession->SetContentListener(nullptr);
        }
        mCallbacks = QMozChromeSessionCallbacks();
    }

    bool loadURL(const QString &url, bool fromExternal) override
    {
        const QByteArray encoded = url.toUtf8();
        return mLegacySession
                && mLegacySession->LoadURL(encoded.constData(), fromExternal);
    }

    bool goBack() override
    {
        return mLegacySession && mLegacySession->GoBack(false, true);
    }

    bool goForward() override
    {
        return mLegacySession && mLegacySession->GoForward(false, true);
    }

    bool stop() override
    {
        return mLegacySession && mLegacySession->StopLoad();
    }

    bool reload(bool hard) override
    {
        return mLegacySession && mLegacySession->Reload(hard);
    }

    bool setActive(bool active) override
    {
        return mLegacySession && mLegacySession->SetActive(active);
    }

    bool setFocused(bool focused) override
    {
        return mLegacySession && mLegacySession->SetFocused(focused);
    }

    bool receiveInputEvent(const EmbedTouchInput &event) override
    {
        return mLegacySession && mLegacySession->ReceiveInputEvent(event);
    }

    bool sendTextEvent(
            const QString &commit, const QString &preedit,
            int replacementStart, int replacementLength) override
    {
        const QByteArray encodedCommit = commit.toUtf8();
        const QByteArray encodedPreedit = preedit.toUtf8();
        return mInputSession
                && mInputSession->SendTextEvent(
                    encodedCommit.constData(), encodedPreedit.constData(),
                    replacementStart, replacementLength);
    }

    bool sendKeyPress(
            int domKeyCode, int modifiers, int charCode) override
    {
        return mInputSession
                && mInputSession->SendKeyPress(
                    domKeyCode, modifiers, charCode);
    }

    bool sendKeyRelease(
            int domKeyCode, int modifiers, int charCode) override
    {
        return mInputSession
                && mInputSession->SendKeyRelease(
                    domKeyCode, modifiers, charCode);
    }

    bool loadFrameScript(const QString &uri) override
    {
        const QByteArray encoded = uri.toUtf8();
        return mContentSession
                && mContentSession->LoadFrameScript(encoded.constData());
    }

    bool addMessageListener(const QByteArray &name) override
    {
        return mContentSession
                && mContentSession->AddMessageListener(name.constData());
    }

    bool removeMessageListener(const QByteArray &name) override
    {
        return mContentSession
                && mContentSession->RemoveMessageListener(name.constData());
    }

    bool sendAsyncMessage(
            quint64 tabId, const QString &name,
            const QString &json) override
    {
        return mContentSession && mContentSession->SendAsyncMessage(
                tabId,
                reinterpret_cast<const char16_t *>(name.utf16()),
                reinterpret_cast<const char16_t *>(json.utf16()));
    }

    bool sendMouseEvent(
            quint64 tabId, QMozChromeMouseType type, qint32 x, qint32 y,
            quint64 time, quint32 button, quint32 buttons,
            quint32 modifiers, quint32 clickCount) override
    {
        const EmbedLiteChromeMouseType embedType = type
                        == QMozChromeMouseType::Move
                ? EmbedLiteChromeMouseType::Move
                : type == QMozChromeMouseType::Down
                ? EmbedLiteChromeMouseType::Down
                : EmbedLiteChromeMouseType::Up;
        return mContentSession && mContentSession->SendMouseEvent(
                tabId, embedType, x, y, time, button, buttons,
                modifiers, clickCount);
    }

    bool sendWheelEvent(
            quint64 tabId, qint32 x, qint32 y, quint64 time,
            double deltaX, double deltaY, quint32 deltaMode,
            quint32 modifiers) override
    {
        return mContentSession && mContentSession->SendWheelEvent(
                tabId, x, y, time, deltaX, deltaY,
                deltaMode, modifiers);
    }

    bool scrollTo(quint64 tabId, qint32 x, qint32 y) override
    {
        return mContentSession && mContentSession->ScrollTo(tabId, x, y);
    }

    bool scrollBy(quint64 tabId, qint32 x, qint32 y) override
    {
        return mContentSession && mContentSession->ScrollBy(tabId, x, y);
    }

    bool zoomToRect(
            quint64 tabId, float x, float y,
            float width, float height) override
    {
        return mContentSession && mContentSession->ZoomToRect(
                tabId, x, y, width, height);
    }

    bool setDesktopMode(quint64 tabId, bool desktopMode) override
    {
        return mContentSession
                && mContentSession->SetDesktopMode(tabId, desktopMode);
    }

    bool setThrottlePainting(quint64 tabId, bool throttle) override
    {
        return mContentSession
                && mContentSession->SetThrottlePainting(tabId, throttle);
    }

    bool suspendTimeouts(quint64 tabId) override
    {
        return mContentSession
                && mContentSession->SuspendTimeouts(tabId);
    }

    bool resumeTimeouts(quint64 tabId) override
    {
        return mContentSession && mContentSession->ResumeTimeouts(tabId);
    }

    bool setHttpUserAgent(
            quint64 tabId, const QString &httpUserAgent) override
    {
        return mContentSession && mContentSession->SetHttpUserAgent(
                tabId, reinterpret_cast<const char16_t *>(
                        httpUserAgent.utf16()));
    }

    bool setMargins(
            quint64 tabId, qint32 top, qint32 right,
            qint32 bottom, qint32 left) override
    {
        return mContentSession && mContentSession->SetMargins(
                tabId, top, right, bottom, left);
    }

    bool setSafeAreaInsets(
            quint64 tabId, qint32 top, qint32 right,
            qint32 bottom, qint32 left) override
    {
        return mContentSession && mContentSession->SetSafeAreaInsets(
                tabId, top, right, bottom, left);
    }

    bool setDynamicToolbarHeight(
            quint64 tabId, qint32 height) override
    {
        return mContentSession
                && mContentSession->SetDynamicToolbarHeight(tabId, height);
    }

    bool setScreenProperties(
            qint32 depth, float density, float dpi) override
    {
        return mContentSession && mContentSession->SetScreenProperties(
                depth, density, dpi);
    }

    bool restoreTabs(const QVector<QMozChromeRestoredTab> &tabs,
                     int selectedTabIndex) override
    {
        if (!mTabSession) {
            return false;
        }

        QVector<QVector<QByteArray> > locations(tabs.count());
        QVector<QVector<QString> > titles(tabs.count());
        QVector<QVector<EmbedLiteChromeHistoryEntry> > history(tabs.count());
        QVector<EmbedLiteChromeRestoredTab> restored(tabs.count());
        for (int tabIndex = 0; tabIndex < tabs.count(); ++tabIndex) {
            const QMozChromeRestoredTab &sourceTab = tabs.at(tabIndex);
            locations[tabIndex].resize(sourceTab.history.count());
            titles[tabIndex].resize(sourceTab.history.count());
            history[tabIndex].resize(sourceTab.history.count());
            for (int historyIndex = 0;
                    historyIndex < sourceTab.history.count(); ++historyIndex) {
                const QMozChromeHistoryEntry &sourceEntry =
                        sourceTab.history.at(historyIndex);
                locations[tabIndex][historyIndex] =
                        sourceEntry.location.toUtf8();
                titles[tabIndex][historyIndex] = sourceEntry.title;
                EmbedLiteChromeHistoryEntry &entry =
                        history[tabIndex][historyIndex];
                entry.location = locations[tabIndex][historyIndex].constData();
                entry.title = reinterpret_cast<const char16_t *>(
                        titles[tabIndex][historyIndex].utf16());
            }

            EmbedLiteChromeRestoredTab &targetTab = restored[tabIndex];
            targetTab.persistentId = sourceTab.persistentId;
            targetTab.history = history[tabIndex].constData();
            targetTab.historyCount = history[tabIndex].count();
            targetTab.selectedHistoryIndex =
                    sourceTab.selectedHistoryIndex;
        }

        return mTabSession->RestoreTabs(restored.constData(), restored.count(),
                                        selectedTabIndex);
    }

    bool newTab(const QString &url, quint64 persistentId,
                bool fromExternal, bool inBackground) override
    {
        const QByteArray encoded = url.toUtf8();
        return mTabSession
                && mTabSession->NewTab(encoded.constData(), persistentId,
                                       fromExternal, inBackground);
    }

    bool associateTab(quint64 tabId, quint64 persistentId) override
    {
        return mTabSession
                && mTabSession->AssociateTab(tabId, persistentId);
    }

    bool selectTab(quint64 tabId) override
    {
        return mTabSession && mTabSession->SelectTab(tabId);
    }

    bool closeTab(quint64 tabId) override
    {
        return mTabSession && mTabSession->CloseTab(tabId);
    }

    bool resolveBeforeUnloadPrompt(
            quint64 requestId, quint64 tabId, bool permit) override
    {
        return mTabSession
                && mTabSession->ResolveBeforeUnloadPrompt(
                    requestId, tabId, permit);
    }

    void OnLocationChanged(const char *location, bool canGoBack,
                           bool canGoForward) override
    {
        const QSharedPointer<EmbedLiteChromeSessionAdapter> self =
                sharedFromThis();
        if (self.isNull()) {
            return;
        }
        const auto callback = mCallbacks.locationChanged;
        if (callback) {
            callback(location, canGoBack, canGoForward);
        }
    }

    void OnLoadStarted(const char *location) override
    {
        const QSharedPointer<EmbedLiteChromeSessionAdapter> self =
                sharedFromThis();
        if (self.isNull()) {
            return;
        }
        const auto callback = mCallbacks.loadStarted;
        if (callback) {
            callback(location);
        }
    }

    void OnLoadFinished() override
    {
        const QSharedPointer<EmbedLiteChromeSessionAdapter> self =
                sharedFromThis();
        if (self.isNull()) {
            return;
        }
        const auto callback = mCallbacks.loadFinished;
        if (callback) {
            callback();
        }
    }

    void OnLoadProgress(int32_t progress, int64_t current,
                        int64_t total) override
    {
        const QSharedPointer<EmbedLiteChromeSessionAdapter> self =
                sharedFromThis();
        if (self.isNull()) {
            return;
        }
        const auto callback = mCallbacks.loadProgress;
        if (callback) {
            callback(progress, current, total);
        }
    }

    void OnTitleChanged(const char16_t *title) override
    {
        const QSharedPointer<EmbedLiteChromeSessionAdapter> self =
                sharedFromThis();
        if (self.isNull()) {
            return;
        }
        const auto callback = mCallbacks.titleChanged;
        if (callback) {
            callback(title);
        }
    }

    void OnTabsChanged(
            uint64_t revision, uint64_t selectedTabId,
            const EmbedLiteChromeTabSnapshot *tabs,
            uint32_t tabCount) override
    {
        const QSharedPointer<EmbedLiteChromeSessionAdapter> self =
                sharedFromThis();
        if (self.isNull()) {
            return;
        }

        if (!revision || (tabCount && !tabs)
                || (!tabCount && selectedTabId)
                || (tabCount && !selectedTabId)) {
            return;
        }

        QVector<QMozChromeTabSnapshot> snapshot;
        snapshot.reserve(tabCount);
        QSet<quint64> runtimeIds;
        QSet<quint64> persistentIds;
        bool selectedFound = false;
        for (uint32_t i = 0; i < tabCount; ++i) {
            const EmbedLiteChromeTabSnapshot &source = tabs[i];
            if (!source.id || runtimeIds.contains(source.id)
                    || (source.persistentId
                        && persistentIds.contains(source.persistentId))) {
                return;
            }
            runtimeIds.insert(source.id);
            if (source.persistentId) {
                persistentIds.insert(source.persistentId);
            }
            selectedFound |= source.id == selectedTabId;

            QMozChromeTabSnapshot target;
            target.id = source.id;
            target.persistentId = source.persistentId;
            target.locationRevision = source.locationRevision;
            target.location = QString::fromUtf8(
                    source.location ? source.location : "");
            target.title = source.title
                    ? QString::fromUtf16(
                        reinterpret_cast<const ushort *>(source.title))
                    : QString();
            target.loading = source.loading;
            target.closing = source.closing;
            target.discarded = source.discarded;
            target.canGoBack = source.canGoBack;
            target.canGoForward = source.canGoForward;
            target.progress = source.progress;
            target.current = source.current;
            target.total = source.total;
            snapshot.append(target);
        }

        if (tabCount && !selectedFound) {
            return;
        }

        const auto callback = mCallbacks.tabsChanged;
        if (callback) {
            callback(revision, selectedTabId, snapshot);
        }
    }

    void OnBeforeUnloadPrompt(
            const EmbedLiteChromeBeforeUnloadPrompt &prompt) override
    {
        const QSharedPointer<EmbedLiteChromeSessionAdapter> self =
                sharedFromThis();
        if (self.isNull() || !prompt.requestId || !prompt.tabId) {
            return;
        }

        QMozChromeBeforeUnloadPrompt copied;
        copied.requestId = prompt.requestId;
        copied.tabId = prompt.tabId;
        copied.persistentId = prompt.persistentId;
        copied.title = prompt.title
                ? QString::fromUtf16(
                    reinterpret_cast<const ushort *>(prompt.title))
                : QString();
        copied.text = prompt.text
                ? QString::fromUtf16(
                    reinterpret_cast<const ushort *>(prompt.text))
                : QString();
        copied.leaveLabel = prompt.leaveLabel
                ? QString::fromUtf16(
                    reinterpret_cast<const ushort *>(prompt.leaveLabel))
                : QString();
        copied.stayLabel = prompt.stayLabel
                ? QString::fromUtf16(
                    reinterpret_cast<const ushort *>(prompt.stayLabel))
                : QString();

        const auto callback = mCallbacks.beforeUnloadPrompt;
        if (callback) {
            callback(copied);
        }
    }

    void OnInputContextChanged(
            int32_t enabled, int32_t open,
            const char16_t *inputType, const char16_t *inputMode,
            const char16_t *actionHint, int32_t cause,
            int32_t focusChange) override
    {
        const QSharedPointer<EmbedLiteChromeSessionAdapter> self =
                sharedFromThis();
        if (self.isNull()) {
            return;
        }

        QMozChromeInputContext context;
        context.enabled = enabled;
        context.open = open;
        context.inputType = inputType
                ? QString::fromUtf16(
                    reinterpret_cast<const ushort *>(inputType))
                : QString();
        context.inputMode = inputMode
                ? QString::fromUtf16(
                    reinterpret_cast<const ushort *>(inputMode))
                : QString();
        context.actionHint = actionHint
                ? QString::fromUtf16(
                    reinterpret_cast<const ushort *>(actionHint))
                : QString();
        context.cause = cause;
        context.focusChange = focusChange;

        const auto callback = mCallbacks.inputContextChanged;
        if (callback) {
            callback(context);
        }
    }

    void OnContentStateChanged(
            const EmbedLiteChromeContentState &source) override
    {
        const QSharedPointer<EmbedLiteChromeSessionAdapter> self =
                sharedFromThis();
        if (self.isNull() || !source.tabId || !source.revision) {
            return;
        }

        QMozChromeContentState state;
        state.tabId = source.tabId;
        state.persistentId = source.persistentId;
        state.locationRevision = source.locationRevision;
        state.revision = source.revision;
        state.securityStatus = QString::fromUtf8(
                source.securityStatus ? source.securityStatus : "");
        state.securityState = source.securityState;
        state.fullscreen = source.fullscreen;
        state.firstPaint = source.firstPaint;
        state.firstPaintX = source.firstPaintX;
        state.firstPaintY = source.firstPaintY;
        state.scrollWidth = source.scrollWidth;
        state.scrollHeight = source.scrollHeight;
        state.scrollX = source.scrollX;
        state.scrollY = source.scrollY;
        state.viewportX = source.viewportX;
        state.viewportY = source.viewportY;
        state.viewportWidth = source.viewportWidth;
        state.viewportHeight = source.viewportHeight;

        const auto callback = mCallbacks.contentStateChanged;
        if (callback) {
            callback(state);
        }
    }

    void RecvAsyncMessage(
            uint64_t tabId, uint64_t persistentId,
            uint64_t locationRevision,
            const char16_t *name, const char16_t *json) override
    {
        const QSharedPointer<EmbedLiteChromeSessionAdapter> self =
                sharedFromThis();
        if (self.isNull() || !tabId || !name || !json) {
            return;
        }
        const auto callback = mCallbacks.asyncMessage;
        if (callback) {
            callback(
                    tabId, persistentId, locationRevision,
                    QString::fromUtf16(
                            reinterpret_cast<const ushort *>(name)),
                    QString::fromUtf16(
                            reinterpret_cast<const ushort *>(json)));
        }
    }

    void OnWindowCloseRequested(
            uint64_t tabId, uint64_t persistentId) override
    {
        const QSharedPointer<EmbedLiteChromeSessionAdapter> self =
                sharedFromThis();
        if (self.isNull() || !tabId) {
            return;
        }
        const auto callback = mCallbacks.windowCloseRequested;
        if (callback) {
            callback(tabId, persistentId);
        }
    }

    void OnTabCloseResult(uint64_t tabId, bool closed) override
    {
        const QSharedPointer<EmbedLiteChromeSessionAdapter> self =
                sharedFromThis();
        if (self.isNull() || !tabId) {
            return;
        }
        const auto callback = mCallbacks.tabCloseResult;
        if (callback) {
            callback(tabId, closed);
        }
    }

    void ChromeSessionDestroyed() override
    {
        const QSharedPointer<EmbedLiteChromeSessionAdapter> self =
                sharedFromThis();
        if (self.isNull()) {
            return;
        }
        mLegacySession = nullptr;
        notifyDestroyedIfComplete();
    }

    void ChromeTabSessionDestroyed() override
    {
        const QSharedPointer<EmbedLiteChromeSessionAdapter> self =
                sharedFromThis();
        if (self.isNull()) {
            return;
        }
        mTabSession = nullptr;
        notifyDestroyedIfComplete();
    }

    void ChromeInputSessionDestroyed() override
    {
        const QSharedPointer<EmbedLiteChromeSessionAdapter> self =
                sharedFromThis();
        if (self.isNull()) {
            return;
        }
        mInputSession = nullptr;
        notifyDestroyedIfComplete();
    }

    void ChromeContentSessionDestroyed() override
    {
        const QSharedPointer<EmbedLiteChromeSessionAdapter> self =
                sharedFromThis();
        if (self.isNull()) {
            return;
        }
        mContentSession = nullptr;
        notifyDestroyedIfComplete();
    }

private:
    void notifyDestroyedIfComplete()
    {
        if (mLegacySession || mTabSession || mInputSession
                || mContentSession) {
            return;
        }
        const auto callback = mCallbacks.destroyed;
        mCallbacks = QMozChromeSessionCallbacks();
        if (callback) {
            callback();
        }
    }

    EmbedLiteChromeSession *mLegacySession;
    EmbedLiteChromeTabSession *mTabSession;
    EmbedLiteChromeInputSession *mInputSession;
    EmbedLiteChromeContentSession *mContentSession;
    const quint32 mUniqueId;
    QMozChromeSessionCallbacks mCallbacks;
};

} // namespace

namespace QtMoz {

QSharedPointer<QMozChromeSession> createEmbedLiteChromeSession(
        const QSharedPointer<QMozSurface> &surface)
{
    EmbedLiteChromeSession *legacySession = nullptr;
    EmbedLiteChromeTabSession *tabSession = nullptr;
    EmbedLiteChromeInputSession *inputSession = nullptr;
    EmbedLiteChromeContentSession *contentSession = nullptr;
    quint32 uniqueId = 0;
    if (!withEmbedLiteWindow(surface, [&](EmbedLiteWindow *window) {
        legacySession = window->GetChromeSession();
        tabSession = window->GetChromeTabSession();
        inputSession = window->GetChromeInputSession();
        contentSession = window->GetChromeContentSession();
        uniqueId = window->GetUniqueID();
    }) || !legacySession || !tabSession || !inputSession || !contentSession
            || uniqueId == 0) {
        return QSharedPointer<QMozChromeSession>();
    }

    const QSharedPointer<EmbedLiteChromeSessionAdapter> adapter(
            new EmbedLiteChromeSessionAdapter(
                legacySession, tabSession, inputSession, contentSession,
                uniqueId));
    return adapter;
}

} // namespace QtMoz

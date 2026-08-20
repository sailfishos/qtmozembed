/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef QMOZCHROMESESSION_P_H
#define QMOZCHROMESESSION_P_H

#include <QtGlobal>

#include <QString>
#include <QVector>

#include <functional>

class QMozWindow;
namespace mozilla {
namespace embedlite {
class EmbedTouchInput;
}
}

struct QMozChromeHistoryEntry final
{
    QString location;
    QString title;
};

struct QMozChromeRestoredTab final
{
    quint64 persistentId;
    QVector<QMozChromeHistoryEntry> history;
    int selectedHistoryIndex;
};

struct QMozChromeTabSnapshot final
{
    quint64 id;
    quint64 persistentId;
    quint64 locationRevision;
    QString location;
    QString title;
    bool loading;
    bool closing;
    bool discarded;
    bool canGoBack;
    bool canGoForward;
    int progress;
    qint64 current;
    qint64 total;
};

struct QMozChromeBeforeUnloadPrompt final
{
    quint64 requestId;
    quint64 tabId;
    quint64 persistentId;
    QString title;
    QString text;
    QString leaveLabel;
    QString stayLabel;
};

struct QMozChromeInputContext final
{
    int enabled;
    int open;
    QString inputType;
    QString inputMode;
    QString actionHint;
    int cause;
    int focusChange;
};

struct QMozChromeSessionCallbacks final
{
    std::function<void(const char *, bool, bool)> locationChanged;
    std::function<void(const char *)> loadStarted;
    std::function<void()> loadFinished;
    std::function<void(int, qint64, qint64)> loadProgress;
    std::function<void(const char16_t *)> titleChanged;
    std::function<void(quint64, quint64,
                       const QVector<QMozChromeTabSnapshot> &)> tabsChanged;
    std::function<void(const QMozChromeBeforeUnloadPrompt &)>
            beforeUnloadPrompt;
    std::function<void(const QMozChromeInputContext &)>
            inputContextChanged;
    std::function<void()> destroyed;
};

class Q_DECL_HIDDEN QMozChromeSession
{
public:
    virtual ~QMozChromeSession() {}

    virtual quint32 uniqueId() const = 0;
    virtual void setCallbacks(
            const QMozChromeSessionCallbacks &callbacks) = 0;
    virtual void clearCallbacks() = 0;
    virtual bool loadURL(const QString &url, bool fromExternal) = 0;
    virtual bool goBack() = 0;
    virtual bool goForward() = 0;
    virtual bool stop() = 0;
    virtual bool reload(bool hard) = 0;
    virtual bool setActive(bool active) = 0;
    virtual bool setFocused(bool focused) = 0;
    virtual bool receiveInputEvent(
            const mozilla::embedlite::EmbedTouchInput &event) = 0;
    virtual bool sendTextEvent(
            const QString &commit, const QString &preedit,
            int replacementStart, int replacementLength) = 0;
    virtual bool sendKeyPress(
            int domKeyCode, int modifiers, int charCode) = 0;
    virtual bool sendKeyRelease(
            int domKeyCode, int modifiers, int charCode) = 0;
    virtual bool restoreTabs(
            const QVector<QMozChromeRestoredTab> &tabs,
            int selectedTabIndex) = 0;
    virtual bool newTab(const QString &url, quint64 persistentId,
                        bool fromExternal, bool inBackground) = 0;
    virtual bool associateTab(quint64 tabId, quint64 persistentId) = 0;
    virtual bool selectTab(quint64 tabId) = 0;
    virtual bool closeTab(quint64 tabId) = 0;
    virtual bool resolveBeforeUnloadPrompt(
            quint64 requestId, quint64 tabId, bool permit) = 0;
};

namespace QtMoz {

Q_DECL_HIDDEN bool attachChromeSession(
        const void *consumer, QMozWindow *window,
        const QMozChromeSessionCallbacks &callbacks);
Q_DECL_HIDDEN void detachChromeSession(const void *consumer);
Q_DECL_HIDDEN quint32 chromeSessionUniqueId(const void *consumer);
Q_DECL_HIDDEN bool chromeSessionLoadURL(
        const void *consumer, const QString &url, bool fromExternal);
Q_DECL_HIDDEN bool chromeSessionGoBack(const void *consumer);
Q_DECL_HIDDEN bool chromeSessionGoForward(const void *consumer);
Q_DECL_HIDDEN bool chromeSessionStop(const void *consumer);
Q_DECL_HIDDEN bool chromeSessionReload(const void *consumer, bool hard);
Q_DECL_HIDDEN bool chromeSessionSetActive(
        const void *consumer, bool active);
Q_DECL_HIDDEN bool chromeSessionSetFocused(
        const void *consumer, bool focused);
Q_DECL_HIDDEN bool chromeSessionReceiveInputEvent(
        const void *consumer,
        const mozilla::embedlite::EmbedTouchInput &event);
Q_DECL_HIDDEN bool chromeSessionSendTextEvent(
        const void *consumer, const QString &commit,
        const QString &preedit, int replacementStart,
        int replacementLength);
Q_DECL_HIDDEN bool chromeSessionSendKeyPress(
        const void *consumer, int domKeyCode, int modifiers,
        int charCode);
Q_DECL_HIDDEN bool chromeSessionSendKeyRelease(
        const void *consumer, int domKeyCode, int modifiers,
        int charCode);
Q_DECL_HIDDEN bool chromeSessionRestoreTabs(
        const void *consumer,
        const QVector<QMozChromeRestoredTab> &tabs,
        int selectedTabIndex);
Q_DECL_HIDDEN bool chromeSessionNewTab(
        const void *consumer, const QString &url, quint64 persistentId,
        bool fromExternal, bool inBackground);
Q_DECL_HIDDEN bool chromeSessionAssociateTab(
        const void *consumer, quint64 tabId, quint64 persistentId);
Q_DECL_HIDDEN bool chromeSessionSelectTab(
        const void *consumer, quint64 tabId);
Q_DECL_HIDDEN bool chromeSessionCloseTab(
        const void *consumer, quint64 tabId);
Q_DECL_HIDDEN bool chromeSessionResolveBeforeUnloadPrompt(
        const void *consumer, quint64 requestId, quint64 tabId,
        bool permit);

} // namespace QtMoz

#endif // QMOZCHROMESESSION_P_H

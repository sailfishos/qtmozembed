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

#include <QByteArray>
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
    quint64 openerId;
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

enum class QMozChromeMouseType : quint8 {
    Move,
    Down,
    Up
};

struct QMozChromeContentState final
{
    quint64 tabId;
    quint64 persistentId;
    quint64 locationRevision;
    quint64 revision;
    QString securityStatus;
    quint32 securityState;
    bool fullscreen;
    bool firstPaint;
    qint32 firstPaintX;
    qint32 firstPaintY;
    quint32 scrollWidth;
    quint32 scrollHeight;
    qint32 scrollX;
    qint32 scrollY;
    double viewportX;
    double viewportY;
    double viewportWidth;
    double viewportHeight;
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
    std::function<void(quint64, bool)> tabCloseResult;
    std::function<void(const QMozChromeInputContext &)>
            inputContextChanged;
    std::function<void(const QMozChromeContentState &)>
            contentStateChanged;
    std::function<void(quint64, quint64, quint64,
                       const QString &, const QString &)> asyncMessage;
    std::function<void(quint64, quint64)> windowCloseRequested;
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
    virtual bool sendTextEventAtOffset(
            const QString &commit, const QString &preedit,
            quint32 replacementOffset, int replacementLength) = 0;
    virtual bool sendKeyPress(
            int domKeyCode, int modifiers, int charCode) = 0;
    virtual bool sendKeyRelease(
            int domKeyCode, int modifiers, int charCode) = 0;
    virtual bool loadFrameScript(const QString &uri) = 0;
    virtual bool addMessageListener(const QByteArray &name) = 0;
    virtual bool removeMessageListener(const QByteArray &name) = 0;
    virtual bool sendAsyncMessage(
            quint64 tabId, const QString &name, const QString &json) = 0;
    virtual bool sendMouseEvent(
            quint64 tabId, QMozChromeMouseType type, qint32 x, qint32 y,
            quint64 time, quint32 button, quint32 buttons,
            quint32 modifiers, quint32 clickCount) = 0;
    virtual bool sendWheelEvent(
            quint64 tabId, qint32 x, qint32 y, quint64 time,
            double deltaX, double deltaY, quint32 deltaMode,
            quint32 modifiers) = 0;
    virtual bool scrollTo(quint64 tabId, qint32 x, qint32 y) = 0;
    virtual bool scrollBy(quint64 tabId, qint32 x, qint32 y) = 0;
    virtual bool zoomToRect(
            quint64 tabId, float x, float y, float width, float height) = 0;
    virtual bool setDesktopMode(quint64 tabId, bool desktopMode) = 0;
    virtual bool setJavascriptEnabled(bool enabled) = 0;
    virtual bool setThrottlePainting(quint64 tabId, bool throttle) = 0;
    virtual bool suspendTimeouts(quint64 tabId) = 0;
    virtual bool resumeTimeouts(quint64 tabId) = 0;
    virtual bool setHttpUserAgent(
            quint64 tabId, const QString &httpUserAgent) = 0;
    virtual bool setMargins(
            quint64 tabId, qint32 top, qint32 right,
            qint32 bottom, qint32 left) = 0;
    virtual bool setSafeAreaInsets(
            quint64 tabId, qint32 top, qint32 right,
            qint32 bottom, qint32 left) = 0;
    virtual bool setDynamicToolbarHeight(
            quint64 tabId, qint32 height) = 0;
    virtual bool setScreenProperties(
            qint32 depth, float density, float dpi) = 0;
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
Q_DECL_HIDDEN bool chromeSessionSendTextEventAtOffset(
        const void *consumer, const QString &commit,
        const QString &preedit, quint32 replacementOffset,
        int replacementLength);
Q_DECL_HIDDEN bool chromeSessionSendKeyPress(
        const void *consumer, int domKeyCode, int modifiers,
        int charCode);
Q_DECL_HIDDEN bool chromeSessionSendKeyRelease(
        const void *consumer, int domKeyCode, int modifiers,
        int charCode);
Q_DECL_HIDDEN bool chromeSessionLoadFrameScript(
        const void *consumer, const QString &uri);
Q_DECL_HIDDEN bool chromeSessionAddMessageListener(
        const void *consumer, const QByteArray &name);
Q_DECL_HIDDEN bool chromeSessionRemoveMessageListener(
        const void *consumer, const QByteArray &name);
Q_DECL_HIDDEN bool chromeSessionSendAsyncMessage(
        const void *consumer, quint64 tabId,
        const QString &name, const QString &json);
Q_DECL_HIDDEN bool chromeSessionSendMouseEvent(
        const void *consumer, quint64 tabId, QMozChromeMouseType type,
        qint32 x, qint32 y, quint64 time, quint32 button,
        quint32 buttons, quint32 modifiers, quint32 clickCount);
Q_DECL_HIDDEN bool chromeSessionSendWheelEvent(
        const void *consumer, quint64 tabId, qint32 x, qint32 y,
        quint64 time, double deltaX, double deltaY,
        quint32 deltaMode, quint32 modifiers);
Q_DECL_HIDDEN bool chromeSessionScrollTo(
        const void *consumer, quint64 tabId, qint32 x, qint32 y);
Q_DECL_HIDDEN bool chromeSessionScrollBy(
        const void *consumer, quint64 tabId, qint32 x, qint32 y);
Q_DECL_HIDDEN bool chromeSessionZoomToRect(
        const void *consumer, quint64 tabId, float x, float y,
        float width, float height);
Q_DECL_HIDDEN bool chromeSessionSetDesktopMode(
        const void *consumer, quint64 tabId, bool desktopMode);
Q_DECL_HIDDEN bool chromeSessionSetJavascriptEnabled(
        const void *consumer, bool enabled);
Q_DECL_HIDDEN bool chromeSessionSetThrottlePainting(
        const void *consumer, quint64 tabId, bool throttle);
Q_DECL_HIDDEN bool chromeSessionSuspendTimeouts(
        const void *consumer, quint64 tabId);
Q_DECL_HIDDEN bool chromeSessionResumeTimeouts(
        const void *consumer, quint64 tabId);
Q_DECL_HIDDEN bool chromeSessionSetHttpUserAgent(
        const void *consumer, quint64 tabId,
        const QString &httpUserAgent);
Q_DECL_HIDDEN bool chromeSessionSetMargins(
        const void *consumer, quint64 tabId, qint32 top, qint32 right,
        qint32 bottom, qint32 left);
Q_DECL_HIDDEN bool chromeSessionSetSafeAreaInsets(
        const void *consumer, quint64 tabId, qint32 top, qint32 right,
        qint32 bottom, qint32 left);
Q_DECL_HIDDEN bool chromeSessionSetDynamicToolbarHeight(
        const void *consumer, quint64 tabId, qint32 height);
Q_DECL_HIDDEN bool chromeSessionSetScreenProperties(
        const void *consumer, qint32 depth, float density, float dpi);
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

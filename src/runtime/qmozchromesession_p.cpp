/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "qmozchromesession_p.h"

#include "../backends/embedlite/embedlitechromesession_p.h"
#include "qmozsurface_p.h"

#include <QCoreApplication>
#include <QHash>
#include <QSharedPointer>
#include <QThread>

namespace {

struct ChromeSessionRegistry
{
    struct Entry {
        QSharedPointer<QMozChromeSession> session;
        quint64 generation;
    };

    QHash<const void *, Entry> sessions;
    quint64 nextGeneration = 1;
};

Q_GLOBAL_STATIC(ChromeSessionRegistry, chromeSessionRegistry)

void assertOwnerThread()
{
    Q_ASSERT(!QCoreApplication::instance()
             || QThread::currentThread()
                == QCoreApplication::instance()->thread());
}

QSharedPointer<QMozChromeSession> chromeSession(const void *consumer)
{
    assertOwnerThread();
    ChromeSessionRegistry * const registry = chromeSessionRegistry();
    return registry && consumer ? registry->sessions.value(consumer).session
                                : QSharedPointer<QMozChromeSession>();
}

} // namespace

namespace QtMoz {

bool attachChromeSession(const void *consumer, QMozWindow *window,
                         const QMozChromeSessionCallbacks &callbacks)
{
    assertOwnerThread();
    ChromeSessionRegistry * const registry = chromeSessionRegistry();
    if (!registry || !consumer || !window
            || registry->sessions.contains(consumer)) {
        return false;
    }

    const QSharedPointer<QMozChromeSession> session =
            createEmbedLiteChromeSession(windowSurface(window));
    if (!session) {
        return false;
    }

    QMozChromeSessionCallbacks registeredCallbacks = callbacks;
    const std::function<void()> destroyed = callbacks.destroyed;
    const quint64 generation = registry->nextGeneration++;
    registeredCallbacks.destroyed = [consumer, generation, destroyed]() {
        assertOwnerThread();
        ChromeSessionRegistry * const registry = chromeSessionRegistry();
        bool currentGeneration = false;
        if (registry) {
            const auto it = registry->sessions.find(consumer);
            if (it != registry->sessions.end()
                    && it.value().generation == generation) {
                registry->sessions.erase(it);
                currentGeneration = true;
            }
        }
        if (currentGeneration && destroyed) {
            destroyed();
        }
    };

    ChromeSessionRegistry::Entry entry;
    entry.session = session;
    entry.generation = generation;
    registry->sessions.insert(consumer, entry);
    session->setCallbacks(registeredCallbacks);
    return registry->sessions.value(consumer).session == session;
}

void detachChromeSession(const void *consumer)
{
    assertOwnerThread();
    ChromeSessionRegistry * const registry = chromeSessionRegistry();
    if (!registry || !consumer) {
        return;
    }
    const QSharedPointer<QMozChromeSession> session =
            registry->sessions.take(consumer).session;
    if (session) {
        session->clearCallbacks();
    }
}

quint32 chromeSessionUniqueId(const void *consumer)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session ? session->uniqueId() : 0;
}

bool chromeSessionLoadURL(const void *consumer, const QString &url,
                          bool fromExternal)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->loadURL(url, fromExternal);
}

bool chromeSessionGoBack(const void *consumer)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->goBack();
}

bool chromeSessionGoForward(const void *consumer)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->goForward();
}

bool chromeSessionStop(const void *consumer)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->stop();
}

bool chromeSessionReload(const void *consumer, bool hard)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->reload(hard);
}

bool chromeSessionSetActive(const void *consumer, bool active)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->setActive(active);
}

bool chromeSessionSetFocused(const void *consumer, bool focused)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->setFocused(focused);
}

bool chromeSessionReceiveInputEvent(
        const void *consumer,
        const mozilla::embedlite::EmbedTouchInput &event)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->receiveInputEvent(event);
}

bool chromeSessionSendTextEvent(
        const void *consumer, const QString &commit,
        const QString &preedit, int replacementStart,
        int replacementLength)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->sendTextEvent(
            commit, preedit, replacementStart, replacementLength);
}

bool chromeSessionSendTextEventAtOffset(
        const void *consumer, const QString &commit,
        const QString &preedit, quint32 replacementOffset,
        int replacementLength)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->sendTextEventAtOffset(
            commit, preedit, replacementOffset, replacementLength);
}

bool chromeSessionSendKeyPress(
        const void *consumer, int domKeyCode, int modifiers,
        int charCode)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session
            && session->sendKeyPress(domKeyCode, modifiers, charCode);
}

bool chromeSessionSendKeyRelease(
        const void *consumer, int domKeyCode, int modifiers,
        int charCode)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session
            && session->sendKeyRelease(domKeyCode, modifiers, charCode);
}

bool chromeSessionLoadFrameScript(const void *consumer, const QString &uri)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->loadFrameScript(uri);
}

bool chromeSessionAddMessageListener(
        const void *consumer, const QByteArray &name)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->addMessageListener(name);
}

bool chromeSessionRemoveMessageListener(
        const void *consumer, const QByteArray &name)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->removeMessageListener(name);
}

bool chromeSessionSendAsyncMessage(
        const void *consumer, quint64 tabId,
        const QString &name, const QString &json)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->sendAsyncMessage(tabId, name, json);
}

bool chromeSessionSendMouseEvent(
        const void *consumer, quint64 tabId, QMozChromeMouseType type,
        qint32 x, qint32 y, quint64 time, quint32 button,
        quint32 buttons, quint32 modifiers, quint32 clickCount)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->sendMouseEvent(
            tabId, type, x, y, time, button, buttons,
            modifiers, clickCount);
}

bool chromeSessionSendWheelEvent(
        const void *consumer, quint64 tabId, qint32 x, qint32 y,
        quint64 time, double deltaX, double deltaY,
        quint32 deltaMode, quint32 modifiers)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->sendWheelEvent(
            tabId, x, y, time, deltaX, deltaY, deltaMode, modifiers);
}

bool chromeSessionScrollTo(
        const void *consumer, quint64 tabId, qint32 x, qint32 y)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->scrollTo(tabId, x, y);
}

bool chromeSessionScrollBy(
        const void *consumer, quint64 tabId, qint32 x, qint32 y)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->scrollBy(tabId, x, y);
}

bool chromeSessionZoomToRect(
        const void *consumer, quint64 tabId, float x, float y,
        float width, float height)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->zoomToRect(tabId, x, y, width, height);
}

bool chromeSessionSetDesktopMode(
        const void *consumer, quint64 tabId, bool desktopMode)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->setDesktopMode(tabId, desktopMode);
}

bool chromeSessionSetJavascriptEnabled(const void *consumer, bool enabled)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->setJavascriptEnabled(enabled);
}

bool chromeSessionSetThrottlePainting(
        const void *consumer, quint64 tabId, bool throttle)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->setThrottlePainting(tabId, throttle);
}

bool chromeSessionSuspendTimeouts(const void *consumer, quint64 tabId)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->suspendTimeouts(tabId);
}

bool chromeSessionResumeTimeouts(const void *consumer, quint64 tabId)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->resumeTimeouts(tabId);
}

bool chromeSessionSetHttpUserAgent(
        const void *consumer, quint64 tabId,
        const QString &httpUserAgent)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->setHttpUserAgent(tabId, httpUserAgent);
}

bool chromeSessionSetMargins(
        const void *consumer, quint64 tabId, qint32 top, qint32 right,
        qint32 bottom, qint32 left)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->setMargins(
            tabId, top, right, bottom, left);
}

bool chromeSessionSetSafeAreaInsets(
        const void *consumer, quint64 tabId, qint32 top, qint32 right,
        qint32 bottom, qint32 left)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->setSafeAreaInsets(
            tabId, top, right, bottom, left);
}

bool chromeSessionSetDynamicToolbarHeight(
        const void *consumer, quint64 tabId, qint32 height)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->setDynamicToolbarHeight(tabId, height);
}

bool chromeSessionSetScreenProperties(
        const void *consumer, qint32 depth, float density, float dpi)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->setScreenProperties(depth, density, dpi);
}

bool chromeSessionRestoreTabs(
        const void *consumer,
        const QVector<QMozChromeRestoredTab> &tabs,
        int selectedTabIndex)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->restoreTabs(tabs, selectedTabIndex);
}

bool chromeSessionNewTab(const void *consumer, const QString &url,
                         quint64 persistentId, bool fromExternal,
                         bool inBackground)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->newTab(url, persistentId, fromExternal,
                                      inBackground);
}

bool chromeSessionAssociateTab(const void *consumer, quint64 tabId,
                               quint64 persistentId)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->associateTab(tabId, persistentId);
}

bool chromeSessionSelectTab(const void *consumer, quint64 tabId)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->selectTab(tabId);
}

bool chromeSessionCloseTab(const void *consumer, quint64 tabId)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->closeTab(tabId);
}

bool chromeSessionResolveBeforeUnloadPrompt(
        const void *consumer, quint64 requestId, quint64 tabId,
        bool permit)
{
    const QSharedPointer<QMozChromeSession> session = chromeSession(consumer);
    return session && session->resolveBeforeUnloadPrompt(
            requestId, tabId, permit);
}

} // namespace QtMoz

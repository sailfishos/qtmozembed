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

} // namespace QtMoz

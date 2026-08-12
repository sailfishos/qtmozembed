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
    QHash<const void *, QSharedPointer<QMozChromeSession>> sessions;
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
    return registry && consumer ? registry->sessions.value(consumer)
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
    registeredCallbacks.destroyed = [consumer, destroyed]() {
        assertOwnerThread();
        ChromeSessionRegistry * const registry = chromeSessionRegistry();
        if (registry) {
            registry->sessions.take(consumer);
        }
        if (destroyed) {
            destroyed();
        }
    };

    registry->sessions.insert(consumer, session);
    session->setCallbacks(registeredCallbacks);
    return registry->sessions.value(consumer) == session;
}

void detachChromeSession(const void *consumer)
{
    assertOwnerThread();
    ChromeSessionRegistry * const registry = chromeSessionRegistry();
    if (!registry || !consumer) {
        return;
    }
    const QSharedPointer<QMozChromeSession> session =
            registry->sessions.take(consumer);
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

} // namespace QtMoz

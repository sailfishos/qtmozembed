/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "qmozchromewindowregistry_p.h"

#include "../qmozcontext.h"
#include "../qmozwindow.h"

#include <QCoreApplication>
#include <QHash>
#include <QPointer>
#include <QSharedPointer>
#include <QThread>
#include <QWeakPointer>

namespace {

struct ChromeWindowContext
{
    QtMoz::QMozChromeWindowShutdown shutdown;
};

struct ChromeWindowRegistry
{
    QHash<QMozContext *, QSharedPointer<ChromeWindowContext>> contexts;
};

Q_GLOBAL_STATIC(ChromeWindowRegistry, chromeWindowRegistry)

void assertOwnerThread()
{
    Q_ASSERT(!QCoreApplication::instance()
             || QThread::currentThread()
                == QCoreApplication::instance()->thread());
}

} // namespace

namespace QtMoz {

bool trackChromeWindow(
        QMozContext *context, QMozWindow *window,
        const QMozChromeWindowDrainRequest &drainRequest)
{
    assertOwnerThread();
    ChromeWindowRegistry * const registry = chromeWindowRegistry();
    if (!registry || !context || !window || !window->isReserved()
            || !drainRequest) {
        return false;
    }

    QSharedPointer<ChromeWindowContext> state =
            registry->contexts.value(context);
    if (!state) {
        state.reset(new ChromeWindowContext);
        registry->contexts.insert(context, state);
        QObject::connect(context, &QObject::destroyed,
                         QCoreApplication::instance(), [context]() {
            ChromeWindowRegistry * const registry = chromeWindowRegistry();
            if (registry) {
                registry->contexts.remove(context);
            }
        });
    }
    if (state->shutdown.isStopping()) {
        return false;
    }

    const QPointer<QMozWindow> guardedWindow(window);
    const QWeakPointer<ChromeWindowContext> weakState(state);
    if (!state->shutdown.track(window, drainRequest,
            [weakState, guardedWindow, window]() {
        if (guardedWindow && guardedWindow->isReserved()) {
            guardedWindow->release();
        } else {
            const QSharedPointer<ChromeWindowContext> state =
                    weakState.toStrongRef();
            if (state) {
                state->shutdown.windowReleased(window);
            }
        }
    })) {
        return false;
    }
    QObject::connect(window, &QMozWindow::released, window,
                     [state, window]() {
        assertOwnerThread();
        state->shutdown.windowReleased(window);
        window->deleteLater();
    });
    return true;
}

bool drainTrackedChromeWindow(QMozContext *context, QMozWindow *window)
{
    assertOwnerThread();
    ChromeWindowRegistry * const registry = chromeWindowRegistry();
    const QSharedPointer<ChromeWindowContext> state =
            registry && context ? registry->contexts.value(context)
                                : QSharedPointer<ChromeWindowContext>();
    return state && window && state->shutdown.beginDrain(window);
}

bool hasTrackedChromeWindows(QMozContext *context)
{
    assertOwnerThread();
    ChromeWindowRegistry * const registry = chromeWindowRegistry();
    const QSharedPointer<ChromeWindowContext> state =
            registry && context ? registry->contexts.value(context)
                                : QSharedPointer<ChromeWindowContext>();
    return state && state->shutdown.isWaiting();
}

bool releaseTrackedChromeWindows(QMozContext *context)
{
    assertOwnerThread();
    ChromeWindowRegistry * const registry = chromeWindowRegistry();
    const QSharedPointer<ChromeWindowContext> state =
            registry && context ? registry->contexts.value(context)
                                : QSharedPointer<ChromeWindowContext>();
    return state && state->shutdown.beginStop();
}

} // namespace QtMoz

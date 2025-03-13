/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#define LOG_COMPONENT "MessagePumpQt"

#include <QTimer>
#include <QEvent>
#include <QThread>
#include <QAbstractEventDispatcher>
#include <QGuiApplication>

#include "qmessagepump.h"

#include "mozilla/embedlite/EmbedLiteMessagePump.h"
#include "mozilla/embedlite/EmbedLiteApp.h"

using namespace mozilla::embedlite;

// Cached QEvent user type, registered for our event system
static int sPokeEvent = -1;

MessagePumpQt::MessagePumpQt(EmbedLiteApp *aApp)
    : mApp(aApp)
    , mTimer(new QTimer(this))
    , mState(nullptr)
    , mLastDelayedWorkTime(-1)
{
    mEventLoopPrivate = mApp->CreateEmbedLiteMessagePump(this);

    // Register our custom event type, to use in qApp event loop
    if (sPokeEvent == -1) {
        sPokeEvent = QEvent::registerEventType();
    }
    connect(mTimer, &QTimer::timeout, this, &MessagePumpQt::dispatchDelayed);
    mTimer->setSingleShot(true);
}

MessagePumpQt::~MessagePumpQt()
{
    mTimer->stop();
    delete mTimer;
    delete mState;
    delete mEventLoopPrivate;
}

bool MessagePumpQt::event(QEvent *e)
{
    if (e->type() == sPokeEvent) {
        handleDispatch();
        return true;
    }
    return QObject::event(e);
}

void MessagePumpQt::handleDispatch()
{
    if (mState->should_quit) {
        return;
    }

    if (mEventLoopPrivate->DoWork(mState->delegate)) {
        // there might be more, see more_work_is_plausible
        // variable above, that's why we ScheduleWork() to keep going.
        scheduleWorkLocal();
    }

    if (mState->should_quit) {
        return;
    }

    bool doIdleWork = !mEventLoopPrivate->DoDelayedWork(mState->delegate);
    scheduleDelayedIfNeeded();

    if (doIdleWork) {
        if (mEventLoopPrivate->DoIdleWork(mState->delegate)) {
            scheduleWorkLocal();
        }
    }
}

void MessagePumpQt::scheduleWorkLocal()
{
    QCoreApplication::postEvent(this, new QEvent((QEvent::Type)sPokeEvent));
}

void MessagePumpQt::scheduleDelayedIfNeeded()
{
    if (mLastDelayedWorkTime == -1) {
        return;
    }

    if (mTimer->isActive()) {
        mTimer->stop();
    }

    mTimer->start(mLastDelayedWorkTime >= 0 ? mLastDelayedWorkTime : 0);
}

void MessagePumpQt::dispatchDelayed()
{
    handleDispatch();
}

void MessagePumpQt::Run(void *delegate)
{
    RunState *state = new RunState();
    state->delegate = delegate;
    state->should_quit = false;
    state->run_depth = mState ? mState->run_depth + 1 : 1;
    mState = state;
    handleDispatch();
}

void MessagePumpQt::Quit()
{
    if (mState) {
        mState->should_quit = true;
        mState->delegate = nullptr;
    }
}

void MessagePumpQt::ScheduleWork()
{
    scheduleWorkLocal();
}

void MessagePumpQt::ScheduleDelayedWork(const int aDelay)
{
    mLastDelayedWorkTime = aDelay;
    scheduleDelayedIfNeeded();
}

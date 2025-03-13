/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef qmessagepump_h
#define qmessagepump_h

#include <QObject>
#include <QTimer>
#include <QVariant>
#include <QStringList>

#include "mozilla/embedlite/EmbedLiteMessagePump.h"

namespace mozilla {
namespace embedlite {
class EmbedLiteApp;
}
}

class MessagePumpQt : public QObject, public mozilla::embedlite::EmbedLiteMessagePumpListener
{
    Q_OBJECT

public:
    MessagePumpQt(mozilla::embedlite::EmbedLiteApp *aApp);
    ~MessagePumpQt();

    bool event(QEvent *e) override;

    void Run(void *aDelegate) override;
    void Quit() override;
    void ScheduleWork() override;
    void ScheduleDelayedWork(const int aDelay) override;

    mozilla::embedlite::EmbedLiteMessagePump *EmbedLoop()
    {
        return mEventLoopPrivate;
    }

public Q_SLOTS:
    void dispatchDelayed();

private:
    void scheduleWorkLocal();
    void scheduleDelayedIfNeeded();
    void handleDispatch();

    // We may make recursive calls to Run, so we save state that needs to be
    // separate between them in this structure type.
    struct RunState {
        void *delegate;
        // Used to flag that the current Run() invocation should return ASAP.
        bool should_quit;
        // Used to count how many Run() invocations are on the stack.
        int run_depth;
    };

    mozilla::embedlite::EmbedLiteApp *mApp;
    mozilla::embedlite::EmbedLiteMessagePump *mEventLoopPrivate;
    QTimer *mTimer;
    RunState *mState;
    int mLastDelayedWorkTime;
    bool mStarted;
};

#endif /* qmessagepump_h */

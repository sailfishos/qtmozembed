/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef QMOZCHROMEWINDOWSHUTDOWN_P_H
#define QMOZCHROMEWINDOWSHUTDOWN_P_H

#include <QtGlobal>
#include <QSharedPointer>

#include <functional>
#include <map>

namespace QtMoz {

using QMozChromeWindowDrainComplete = std::function<void()>;
using QMozChromeWindowDrainRequest = std::function<void(
        const QMozChromeWindowDrainComplete &)>;
using QMozChromeWindowReleaseRequest = std::function<void()>;

// Owner-thread state machine which prevents a native chrome window from
// being released until its render-thread frame lease has been drained.
class Q_DECL_HIDDEN QMozChromeWindowShutdown final
{
public:
    QMozChromeWindowShutdown() = default;

    bool track(const void *window,
               const QMozChromeWindowDrainRequest &drainRequest,
               const QMozChromeWindowReleaseRequest &releaseRequest);
    bool beginDrain(const void *window);
    bool beginStop();
    bool windowReleased(const void *window);
    bool contains(const void *window) const;
    bool isWaiting() const;
    bool isStopping() const;

private:
    struct Entry
    {
        QMozChromeWindowDrainRequest drainRequest;
        QMozChromeWindowReleaseRequest releaseRequest;
        bool draining = false;
        bool releaseIssued = false;
    };
    struct State
    {
        std::map<const void *, Entry> entries;
        bool stopping = false;
    };

    static void drainFinished(
            const QSharedPointer<State> &state, const void *window);

    const QSharedPointer<State> mState = QSharedPointer<State>(new State);

    Q_DISABLE_COPY(QMozChromeWindowShutdown)
};

} // namespace QtMoz

#endif // QMOZCHROMEWINDOWSHUTDOWN_P_H

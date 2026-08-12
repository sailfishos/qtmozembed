/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "qmozchromewindowshutdown_p.h"

#include <vector>

namespace QtMoz {

bool QMozChromeWindowShutdown::track(
        const void *window,
        const QMozChromeWindowDrainRequest &drainRequest,
        const QMozChromeWindowReleaseRequest &releaseRequest)
{
    if (!window || !drainRequest || !releaseRequest
            || mState->entries.find(window) != mState->entries.end()) {
        return false;
    }

    Entry entry;
    entry.drainRequest = drainRequest;
    entry.releaseRequest = releaseRequest;
    mState->entries.emplace(window, entry);
    if (mState->stopping) {
        beginDrain(window);
    }
    return true;
}

bool QMozChromeWindowShutdown::beginDrain(const void *window)
{
    auto found = mState->entries.find(window);
    if (found == mState->entries.end()) {
        return false;
    }
    if (found->second.draining || found->second.releaseIssued) {
        return true;
    }

    found->second.draining = true;
    const QMozChromeWindowDrainRequest drainRequest =
            found->second.drainRequest;
    const QSharedPointer<State> state = mState;
    drainRequest([state, window]() {
        drainFinished(state, window);
    });
    return true;
}

bool QMozChromeWindowShutdown::beginStop()
{
    mState->stopping = true;
    std::vector<const void *> windows;
    windows.reserve(mState->entries.size());
    for (const auto &entry : mState->entries) {
        windows.push_back(entry.first);
    }
    for (const void *window : windows) {
        beginDrain(window);
    }
    return !mState->entries.empty();
}

bool QMozChromeWindowShutdown::windowReleased(const void *window)
{
    return mState->entries.erase(window) != 0;
}

bool QMozChromeWindowShutdown::contains(const void *window) const
{
    return mState->entries.find(window) != mState->entries.end();
}

bool QMozChromeWindowShutdown::isWaiting() const
{
    return !mState->entries.empty();
}

bool QMozChromeWindowShutdown::isStopping() const
{
    return mState->stopping;
}

void QMozChromeWindowShutdown::drainFinished(
        const QSharedPointer<State> &state, const void *window)
{
    auto found = state->entries.find(window);
    if (found == state->entries.end() || !found->second.draining
            || found->second.releaseIssued) {
        return;
    }

    found->second.releaseIssued = true;
    const QMozChromeWindowReleaseRequest releaseRequest =
            found->second.releaseRequest;
    releaseRequest();
}

} // namespace QtMoz

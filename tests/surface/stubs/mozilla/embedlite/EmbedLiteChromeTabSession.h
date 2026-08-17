/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TEST_EMBEDLITECHROMETABSESSION_H
#define TEST_EMBEDLITECHROMETABSESSION_H

#include <cstdint>

namespace mozilla {
namespace embedlite {

struct EmbedLiteChromeHistoryEntry
{
    const char *location;
    const char16_t *title;
};

struct EmbedLiteChromeRestoredTab
{
    uint64_t persistentId;
    const EmbedLiteChromeHistoryEntry *history;
    uint32_t historyCount;
    int32_t selectedHistoryIndex;
};

struct EmbedLiteChromeTabSnapshot
{
    uint64_t id;
    uint64_t persistentId;
    uint64_t locationRevision;
    const char *location;
    const char16_t *title;
    bool loading;
    bool closing;
    bool discarded;
    bool canGoBack;
    bool canGoForward;
    int32_t progress;
    int64_t current;
    int64_t total;
};

class EmbedLiteChromeTabSessionListener
{
public:
    virtual void OnTabsChanged(
            uint64_t revision, uint64_t selectedTabId,
            const EmbedLiteChromeTabSnapshot *tabs,
            uint32_t tabCount) = 0;
    virtual void ChromeTabSessionDestroyed() = 0;

protected:
    virtual ~EmbedLiteChromeTabSessionListener() = default;
};

class EmbedLiteChromeTabSession
{
public:
    virtual void SetTabListener(
            EmbedLiteChromeTabSessionListener *listener) = 0;
    virtual bool RestoreTabs(const EmbedLiteChromeRestoredTab *tabs,
                             uint32_t tabCount,
                             int32_t selectedTabIndex) = 0;
    virtual bool NewTab(const char *url, uint64_t persistentId,
                        bool fromExternal, bool inBackground) = 0;
    virtual bool AssociateTab(uint64_t tabId, uint64_t persistentId) = 0;
    virtual bool SelectTab(uint64_t tabId) = 0;
    virtual bool CloseTab(uint64_t tabId) = 0;

protected:
    virtual ~EmbedLiteChromeTabSession() = default;
};

} // namespace embedlite
} // namespace mozilla

#endif // TEST_EMBEDLITECHROMETABSESSION_H

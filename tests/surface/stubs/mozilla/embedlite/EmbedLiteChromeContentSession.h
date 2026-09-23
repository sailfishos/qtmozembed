/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TEST_EMBEDLITECHROMECONTENTSESSION_H
#define TEST_EMBEDLITECHROMECONTENTSESSION_H

#include <cstdint>

namespace mozilla {
namespace embedlite {

enum class EmbedLiteChromeMouseType : uint8_t {
    Move,
    Down,
    Up
};

struct EmbedLiteChromeContentState
{
    uint64_t tabId;
    uint64_t persistentId;
    uint64_t revision;
    uint64_t locationRevision;
    const char *securityStatus;
    uint32_t securityState;
    bool fullscreen;
    bool firstPaint;
    int32_t firstPaintX;
    int32_t firstPaintY;
    uint32_t scrollWidth;
    uint32_t scrollHeight;
    int32_t scrollX;
    int32_t scrollY;
    double viewportX;
    double viewportY;
    double viewportWidth;
    double viewportHeight;
};

class EmbedLiteChromeContentSessionListener
{
public:
    virtual void OnContentStateChanged(
            const EmbedLiteChromeContentState &) {}
    virtual void RecvAsyncMessage(
            uint64_t, uint64_t, uint64_t,
            const char16_t *, const char16_t *) {}
    virtual void OnWindowCloseRequested(uint64_t, uint64_t) {}
    virtual void OnTabCloseResult(uint64_t, bool) {}
    virtual void ChromeContentSessionDestroyed() {}

protected:
    virtual ~EmbedLiteChromeContentSessionListener() = default;
};

class EmbedLiteChromeContentSession
{
public:
    virtual void SetContentListener(
            EmbedLiteChromeContentSessionListener *) = 0;
    virtual bool LoadFrameScript(const char *) = 0;
    virtual bool AddMessageListener(const char *) = 0;
    virtual bool RemoveMessageListener(const char *) = 0;
    virtual bool SendAsyncMessage(
            uint64_t, const char16_t *, const char16_t *) = 0;
    virtual bool SendMouseEvent(
            uint64_t, EmbedLiteChromeMouseType, int32_t, int32_t,
            uint64_t, uint32_t, uint32_t, uint32_t, uint32_t) = 0;
    virtual bool SendWheelEvent(
            uint64_t, int32_t, int32_t, uint64_t, double, double,
            uint32_t, uint32_t) = 0;
    virtual bool ScrollTo(uint64_t, int32_t, int32_t) = 0;
    virtual bool ScrollBy(uint64_t, int32_t, int32_t) = 0;
    virtual bool ZoomToRect(
            uint64_t, float, float, float, float) = 0;
    virtual bool SetDesktopMode(uint64_t, bool) = 0;
    virtual bool SetJavascriptEnabled(bool) = 0;
    virtual bool SetThrottlePainting(uint64_t, bool) = 0;
    virtual bool SuspendTimeouts(uint64_t) = 0;
    virtual bool ResumeTimeouts(uint64_t) = 0;
    virtual bool SetHttpUserAgent(uint64_t, const char16_t *) = 0;
    virtual bool SetMargins(
            uint64_t, int32_t, int32_t, int32_t, int32_t) = 0;
    virtual bool SetSafeAreaInsets(
            uint64_t, int32_t, int32_t, int32_t, int32_t) = 0;
    virtual bool SetDynamicToolbarHeight(uint64_t, int32_t) = 0;
    virtual bool SetScreenProperties(int32_t, float, float) = 0;

protected:
    virtual ~EmbedLiteChromeContentSession() = default;
};

} // namespace embedlite
} // namespace mozilla

#endif // TEST_EMBEDLITECHROMECONTENTSESSION_H

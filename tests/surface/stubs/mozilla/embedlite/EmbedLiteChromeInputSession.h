/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TEST_EMBEDLITECHROMEINPUTSESSION_H
#define TEST_EMBEDLITECHROMEINPUTSESSION_H

#include <cstdint>

namespace mozilla {
namespace embedlite {

class EmbedLiteChromeInputSessionListener
{
public:
    virtual void OnInputContextChanged(
            int32_t enabled, int32_t open,
            const char16_t *inputType, const char16_t *inputMode,
            const char16_t *actionHint, int32_t cause,
            int32_t focusChange) = 0;
    virtual void ChromeInputSessionDestroyed() = 0;

protected:
    virtual ~EmbedLiteChromeInputSessionListener() = default;
};

class EmbedLiteChromeInputSession
{
public:
    virtual void SetInputListener(
            EmbedLiteChromeInputSessionListener *listener) = 0;
    virtual bool SendTextEvent(
            const char *commit, const char *preedit,
            int32_t replacementStart, int32_t replacementLength) = 0;
    virtual bool SendTextEventAtOffset(
            const char *commit, const char *preedit,
            uint32_t replacementOffset, int32_t replacementLength) = 0;
    virtual bool SendKeyPress(
            int32_t domKeyCode, int32_t modifiers,
            int32_t charCode) = 0;
    virtual bool SendKeyRelease(
            int32_t domKeyCode, int32_t modifiers,
            int32_t charCode) = 0;

protected:
    virtual ~EmbedLiteChromeInputSession() = default;
};

} // namespace embedlite
} // namespace mozilla

#endif // TEST_EMBEDLITECHROMEINPUTSESSION_H

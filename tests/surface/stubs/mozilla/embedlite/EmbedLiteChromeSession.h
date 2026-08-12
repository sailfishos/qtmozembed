/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TEST_EMBEDLITECHROMESESSION_H
#define TEST_EMBEDLITECHROMESESSION_H

#include <cstdint>

namespace mozilla {
namespace embedlite {

class EmbedLiteChromeSessionListener
{
public:
    virtual void OnLocationChanged(const char *, bool, bool) {}
    virtual void OnLoadStarted(const char *) {}
    virtual void OnLoadFinished() {}
    virtual void OnLoadProgress(int32_t, int64_t, int64_t) {}
    virtual void OnTitleChanged(const char16_t *) {}
    virtual void ChromeSessionDestroyed() {}

protected:
    virtual ~EmbedLiteChromeSessionListener() = default;
};

class EmbedLiteChromeSession
{
public:
    virtual void SetListener(EmbedLiteChromeSessionListener *) = 0;
    virtual bool LoadURL(const char *, bool) = 0;
    virtual bool GoBack(bool, bool) = 0;
    virtual bool GoForward(bool, bool) = 0;
    virtual bool StopLoad() = 0;
    virtual bool Reload(bool) = 0;
    virtual bool SetActive(bool) = 0;
    virtual bool SetFocused(bool) = 0;

protected:
    virtual ~EmbedLiteChromeSession() = default;
};

} // namespace embedlite
} // namespace mozilla

#endif // TEST_EMBEDLITECHROMESESSION_H

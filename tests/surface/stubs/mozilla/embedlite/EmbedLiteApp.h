/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TEST_EMBEDLITEAPP_H
#define TEST_EMBEDLITEAPP_H

#include "EmbedLiteWindow.h"

namespace mozilla {
namespace embedlite {

class EmbedLiteApp
{
public:
    EmbedLiteApp()
        : window(&events)
        , createCount(0)
        , chromeCreateCount(0)
        , chromeTabCreateCount(0)
        , destroyCount(0)
        , windowListener(nullptr)
    {
    }

    EmbedLiteWindow *CreateWindow(
            int, int, EmbedLiteWindowListener *listener)
    {
        ++createCount;
        window.SetChromeHosted(false);
        windowListener = listener;
        events.push_back("created");
        return &window;
    }

    EmbedLiteWindow *CreateChromeWindow(
            int, int, const char *initialUrl,
            EmbedLiteWindowListener *listener)
    {
        ++chromeCreateCount;
        window.SetChromeHosted(true);
        windowListener = listener;
        chromeInitialUrl = initialUrl ? initialUrl : "";
        events.push_back("chrome-created");
        return &window;
    }

    EmbedLiteWindow *CreateChromeTabWindow(
            int, int, EmbedLiteWindowListener *listener)
    {
        ++chromeTabCreateCount;
        window.SetChromeHosted(true);
        windowListener = listener;
        chromeInitialUrl.clear();
        events.push_back("chrome-tab-created");
        return &window;
    }

    void DestroyWindow(EmbedLiteWindow *destroyedWindow)
    {
        if (destroyedWindow == &window) {
            ++destroyCount;
            events.push_back("destroyed");
        }
    }

    void NotifyChromeWindowInitializationFailed()
    {
        EmbedLiteChromeWindowListener * const chromeListener =
                dynamic_cast<EmbedLiteChromeWindowListener *>(
                    windowListener);
        if (chromeListener) {
            chromeListener->ChromeWindowInitializationFailed();
        }
    }

    std::vector<std::string> events;
    EmbedLiteWindow window;
    int createCount;
    int chromeCreateCount;
    int chromeTabCreateCount;
    int destroyCount;
    EmbedLiteWindowListener *windowListener;
    std::string chromeInitialUrl;
};

} // namespace embedlite
} // namespace mozilla

#endif // TEST_EMBEDLITEAPP_H

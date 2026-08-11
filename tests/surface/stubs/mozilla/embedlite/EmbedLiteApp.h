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
        , destroyCount(0)
    {
    }

    EmbedLiteWindow *CreateWindow(int, int, EmbedLiteWindowListener *)
    {
        ++createCount;
        events.push_back("created");
        return &window;
    }

    void DestroyWindow(EmbedLiteWindow *destroyedWindow)
    {
        if (destroyedWindow == &window) {
            ++destroyCount;
            events.push_back("destroyed");
        }
    }

    std::vector<std::string> events;
    EmbedLiteWindow window;
    int createCount;
    int destroyCount;
};

} // namespace embedlite
} // namespace mozilla

#endif // TEST_EMBEDLITEAPP_H

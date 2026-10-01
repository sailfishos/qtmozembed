/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TEST_EMBEDINPUTDATA_H
#define TEST_EMBEDINPUTDATA_H

#include <cstdint>
#include <vector>

namespace mozilla {
namespace embedlite {

struct TouchPointF
{
    TouchPointF(float xValue, float yValue)
        : x(xValue)
        , y(yValue)
    {
    }

    float x;
    float y;
};

struct TouchData
{
    TouchData(int32_t identifierValue, TouchPointF pointValue,
              float pressureValue)
        : identifier(identifierValue)
        , touchPoint(pointValue)
        , pressure(pressureValue)
    {
    }

    int32_t identifier;
    TouchPointF touchPoint;
    float pressure;
};

class EmbedTouchInput
{
public:
    enum EmbedTouchType {
        MULTITOUCH_START,
        MULTITOUCH_MOVE,
        MULTITOUCH_END,
        MULTITOUCH_CANCEL,
        MULTITOUCH_SENTINEL
    };

    EmbedTouchInput(EmbedTouchType inputType, uint32_t inputTimeStamp)
        : type(inputType)
        , timeStamp(inputTimeStamp)
    {
    }

    EmbedTouchType type;
    uint32_t timeStamp;
    std::vector<TouchData> touches;
};

} // namespace embedlite
} // namespace mozilla

#endif // TEST_EMBEDINPUTDATA_H

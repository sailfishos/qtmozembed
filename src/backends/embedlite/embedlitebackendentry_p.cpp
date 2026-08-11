/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../../runtime/backendapi_p.h"

namespace QtMoz {

const QtMozBackendApiV1 &embedLiteBackendApiV1()
{
    static const QtMozBackendApiV1 api = {
        sizeof(QtMozBackendApiV1),
        QTMOZ_BACKEND_ABI_MAJOR,
        QTMOZ_BACKEND_ABI_MINOR,
        QTMOZ_BACKEND_CAP_NONE,
        "embedlite"
    };
    return api;
}

} // namespace QtMoz

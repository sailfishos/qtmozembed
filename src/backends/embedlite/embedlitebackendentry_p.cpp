/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../../runtime/backendapi_p.h"

#include <algorithm>
#include <cstring>

namespace QtMoz {

const QtMozBackendApiV1 &embedLiteBackendApiV1()
{
    static const QtMozBackendApiV1 api = {
        sizeof(QtMozBackendApiV1),
        QTMOZ_BACKEND_ABI_MAJOR,
        QTMOZ_BACKEND_ABI_MINOR,
        QTMOZ_BACKEND_CAP_NONE,
        "embedlite",
        nullptr
    };
    return api;
}

QtMozBackendResult queryEmbedLiteBackendV1(
        const QtMozBackendHostV1 *host, QtMozBackendApiV1 *api)
{
    if (!api) {
        return QTMOZ_BACKEND_RESULT_INVALID_ARGUMENT;
    }

    const uint32_t capacity = api->struct_size;
    if (capacity < QTMOZ_BACKEND_API_V1_REQUIRED_SIZE) {
        return QTMOZ_BACKEND_RESULT_BUFFER_TOO_SMALL;
    }

    const BackendApiValidation hostValidation = validateBackendHost(host);
    if (!hostValidation.isValid()) {
        return hostValidation.error == BackendApiError::IncompatibleMajor
                ? QTMOZ_BACKEND_RESULT_INCOMPATIBLE_ABI
                : QTMOZ_BACKEND_RESULT_INVALID_ARGUMENT;
    }

    const uint32_t descriptorSize =
            static_cast<uint32_t>(sizeof(QtMozBackendApiV1));
    const uint32_t provided = std::min<uint32_t>(
            capacity, descriptorSize);
    const QtMozBackendApiV1 descriptor = embedLiteBackendApiV1();
    std::memcpy(api, &descriptor, provided);
    api->struct_size = provided;
    return QTMOZ_BACKEND_RESULT_OK;
}

} // namespace QtMoz

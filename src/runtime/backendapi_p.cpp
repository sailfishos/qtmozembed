/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "backendapi_p.h"

#include <cstddef>
#include <type_traits>

namespace {

static_assert(std::is_standard_layout<QtMozBackendHostV1>::value,
              "The backend host table must have a C-compatible layout");
static_assert(std::is_standard_layout<QtMozBackendApiV1>::value,
              "The backend API table must have a C-compatible layout");
static_assert(offsetof(QtMozBackendHostV1, struct_size) == 0,
              "The backend host table size must be its first field");
static_assert(offsetof(QtMozBackendApiV1, struct_size) == 0,
              "The backend API table size must be its first field");
static_assert(QTMOZ_BACKEND_HOST_V1_REQUIRED_SIZE
                      <= sizeof(QtMozBackendHostV1),
              "The required host prefix must fit in its table");
static_assert(QTMOZ_BACKEND_API_V1_REQUIRED_SIZE
                      <= sizeof(QtMozBackendApiV1),
              "The required API prefix must fit in its table");

} // namespace

namespace QtMoz {

BackendApiValidation validateBackendApi(const QtMozBackendApiV1 *api,
                                        uint64_t requiredCapabilities)
{
    BackendApiValidation result = {
        BackendApiError::NullDescriptor,
        0,
        QTMOZ_BACKEND_CAP_NONE
    };

    if (!api) {
        return result;
    }
    if (api->struct_size < QTMOZ_BACKEND_API_V1_REQUIRED_SIZE) {
        result.error = BackendApiError::TruncatedDescriptor;
        return result;
    }
    if (api->abi_major != QTMOZ_BACKEND_ABI_MAJOR) {
        result.error = BackendApiError::IncompatibleMajor;
        return result;
    }

    result.negotiatedMinor = QTMOZ_BACKEND_ABI_MINOR < api->abi_minor
            ? QTMOZ_BACKEND_ABI_MINOR : api->abi_minor;
    result.capabilities = api->capabilities;
    if ((api->capabilities & requiredCapabilities) != requiredCapabilities) {
        result.error = BackendApiError::MissingCapabilities;
        return result;
    }

    result.error = BackendApiError::None;
    return result;
}

const char *backendApiName(const QtMozBackendApiV1 *api)
{
    const size_t nameEnd = offsetof(QtMozBackendApiV1, name)
            + sizeof(api->name);
    return api && api->struct_size >= nameEnd && api->name
            ? api->name : "";
}

} // namespace QtMoz

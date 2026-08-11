/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef QTMOZ_BACKENDAPI_P_H
#define QTMOZ_BACKENDAPI_P_H

#include "backendabi.h"

namespace QtMoz {

#if defined(__GNUC__)
#define QTMOZ_BACKEND_INTERNAL __attribute__((visibility("hidden")))
#else
#define QTMOZ_BACKEND_INTERNAL
#endif

enum class BackendApiError {
    None,
    NullDescriptor,
    TruncatedDescriptor,
    IncompatibleMajor,
    MissingCapabilities
};

struct BackendApiValidation {
    BackendApiError error;
    uint32_t negotiatedMinor;
    uint64_t capabilities;

    bool isValid() const
    {
        return error == BackendApiError::None;
    }
};

QTMOZ_BACKEND_INTERNAL BackendApiValidation validateBackendApi(
        const QtMozBackendApiV1 *api,
        uint64_t requiredCapabilities = QTMOZ_BACKEND_CAP_NONE);

QTMOZ_BACKEND_INTERNAL const char *backendApiName(
        const QtMozBackendApiV1 *api);

/* Internal descriptor for the built-in compatibility backend. */
QTMOZ_BACKEND_INTERNAL const QtMozBackendApiV1 &embedLiteBackendApiV1();

} // namespace QtMoz

#undef QTMOZ_BACKEND_INTERNAL

#endif // QTMOZ_BACKENDAPI_P_H

/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "runtime/backendabi.h"

int qtmoz_backend_c_abi_test(void)
{
    QtMozBackendStringViewV1 message = { 0 };
    QtMozBackendHostV1 host = { 0 };
    QtMozBackendRuntimeCallbacksV1 callbacks = { 0 };
    QtMozBackendRuntimeOpsV1 runtime_ops = { 0 };
    QtMozBackendApiV1 api = { 0 };
    QtMozBackendQueryV1 query = 0;
    QtMozBackendRuntimeHandle runtime = QTMOZ_BACKEND_RUNTIME_INVALID;

    runtime = UINT64_MAX;

    return (int)(sizeof(message) + sizeof(host) + sizeof(callbacks)
                 + sizeof(runtime_ops) + sizeof(api) + (query != 0)
                 + (runtime != QTMOZ_BACKEND_RUNTIME_INVALID));
}

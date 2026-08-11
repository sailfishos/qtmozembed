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
    QtMozBackendHostV1 host = { 0 };
    QtMozBackendApiV1 api = { 0 };
    QtMozBackendQueryV1 query = 0;

    return (int)(sizeof(host) + sizeof(api) + (query != 0));
}

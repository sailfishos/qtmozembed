/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef QTMOZ_BACKEND_ABI_H
#define QTMOZ_BACKEND_ABI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QTMOZ_BACKEND_ABI_MAJOR 1U
#define QTMOZ_BACKEND_ABI_MINOR 0U

#if defined(__GNUC__)
#define QTMOZ_BACKEND_EXPORT __attribute__((visibility("default")))
#else
#define QTMOZ_BACKEND_EXPORT
#endif

#define QTMOZ_BACKEND_CAP_NONE UINT64_C(0)
#define QTMOZ_BACKEND_CAP_RUNTIME_CUSTOM_PUMP_V1 (UINT64_C(1) << 0)
#define QTMOZ_BACKEND_CAP_WINDOWLESS_SESSIONS_V1 (UINT64_C(1) << 1)
#define QTMOZ_BACKEND_CAP_EXTERNAL_EGL_IMAGE_LEASE_V1 (UINT64_C(1) << 2)
#define QTMOZ_BACKEND_CAP_LEGACY_SYNC_MESSAGE_V1 (UINT64_C(1) << 3)
#define QTMOZ_BACKEND_CAP_LEGACY_OPENER_TOKEN_V1 (UINT64_C(1) << 4)

/*
 * Every ABI table starts with its byte size and version. A minor revision may
 * only append fields. Readers must not access fields beyond struct_size.
 */
typedef struct QtMozBackendHostV1 {
    uint32_t struct_size;
    uint32_t abi_major;
    uint32_t abi_minor;
    uint64_t capabilities;
} QtMozBackendHostV1;

typedef struct QtMozBackendApiV1 {
    uint32_t struct_size;
    uint32_t abi_major;
    uint32_t abi_minor;
    uint64_t capabilities;

    /* Optional diagnostic name. It is not used for compatibility decisions. */
    const char *name;
} QtMozBackendApiV1;

#define QTMOZ_BACKEND_HOST_V1_REQUIRED_SIZE \
    (offsetof(QtMozBackendHostV1, capabilities) + sizeof(uint64_t))
#define QTMOZ_BACKEND_API_V1_REQUIRED_SIZE \
    (offsetof(QtMozBackendApiV1, capabilities) + sizeof(uint64_t))

typedef int32_t (*QtMozBackendQueryV1)(const QtMozBackendHostV1 *host,
                                      QtMozBackendApiV1 *api);

/* This symbol belongs to a backend DSO, not to libqt5embedwidget. */
QTMOZ_BACKEND_EXPORT int32_t qtmoz_backend_query_v1(
        const QtMozBackendHostV1 *host, QtMozBackendApiV1 *api);

#ifdef __cplusplus
}
#endif

#undef QTMOZ_BACKEND_EXPORT

#endif // QTMOZ_BACKEND_ABI_H

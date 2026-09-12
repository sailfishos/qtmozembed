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
#define QTMOZ_BACKEND_ABI_MINOR 1U

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
#define QTMOZ_BACKEND_CAP_RUNTIME_LIFECYCLE_V1 (UINT64_C(1) << 5)

#define QTMOZ_BACKEND_HOST_CAP_NONE UINT64_C(0)
#define QTMOZ_BACKEND_HOST_CAP_LOG_V1 (UINT64_C(1) << 0)

typedef int32_t QtMozBackendResult;

#define QTMOZ_BACKEND_RESULT_OK INT32_C(0)
#define QTMOZ_BACKEND_RESULT_PENDING INT32_C(1)
#define QTMOZ_BACKEND_RESULT_INVALID_ARGUMENT INT32_C(-1)
#define QTMOZ_BACKEND_RESULT_INCOMPATIBLE_ABI INT32_C(-2)
#define QTMOZ_BACKEND_RESULT_UNSUPPORTED INT32_C(-3)
#define QTMOZ_BACKEND_RESULT_INVALID_STATE INT32_C(-4)
#define QTMOZ_BACKEND_RESULT_FAILED INT32_C(-5)
#define QTMOZ_BACKEND_RESULT_BUFFER_TOO_SMALL INT32_C(-6)

#define QTMOZ_BACKEND_LOG_DEBUG UINT32_C(0)
#define QTMOZ_BACKEND_LOG_INFO UINT32_C(1)
#define QTMOZ_BACKEND_LOG_WARNING UINT32_C(2)
#define QTMOZ_BACKEND_LOG_CRITICAL UINT32_C(3)

typedef struct QtMozBackendStringViewV1 {
    /* UTF-8 bytes, not necessarily null-terminated. */
    const char *data;
    uint64_t size;
} QtMozBackendStringViewV1;

/* The message view is valid only for the duration of this non-blocking call. */
typedef void (*QtMozBackendLogV1)(
        void *context, uint32_t level,
        const QtMozBackendStringViewV1 *message);

/*
 * Every ABI table starts with its byte size and version. A minor revision may
 * only append fields. Readers must not access fields beyond struct_size.
 */
typedef struct QtMozBackendHostV1 {
    uint32_t struct_size;
    uint32_t abi_major;
    uint32_t abi_minor;
    uint64_t capabilities;

    /* Optional v1.1 logging callback, advertised with HOST_CAP_LOG_V1. */
    void *context;
    QtMozBackendLogV1 log;
} QtMozBackendHostV1;

typedef uint64_t QtMozBackendRuntimeHandle;

#define QTMOZ_BACKEND_RUNTIME_INVALID UINT64_C(0)

/*
 * Runtime lifecycle is serialized and single-use:
 *
 *   create -> start -> initialized -> stop -> destroyed -> destroy
 *
 * create() synchronously creates a stopped runtime. start() and stop() return
 * OK when their transition and callback completed before return, or PENDING
 * when the callback will arrive later. A failed transition has no callback.
 * initialized and destroyed are each called at most once and in that order.
 * They may be called synchronously or from a backend-owned thread, so they
 * must not block or re-enter backend operations.
 *
 * The backend copies the callback table, host table, and their context values
 * during create(); it must not retain pointers to either table. The pointed-to
 * contexts and callback code must remain valid until destroy() returns.
 * create() requires a non-null runtime output, never returns PENDING, and
 * writes a nonzero handle only on OK. Every other result leaves the output set
 * to RUNTIME_INVALID.
 * destroy() is synchronous, is only valid after destroyed (or when start never
 * succeeded), and guarantees that no later callback can occur.
 */
typedef struct QtMozBackendRuntimeCallbacksV1 {
    uint32_t struct_size;
    uint32_t abi_major;
    uint32_t abi_minor;
    void *context;
    void (*initialized)(void *context);
    void (*destroyed)(void *context);
} QtMozBackendRuntimeCallbacksV1;

typedef struct QtMozBackendRuntimeOpsV1 {
    uint32_t struct_size;
    uint32_t abi_major;
    uint32_t abi_minor;

    QtMozBackendResult (*create)(
            const QtMozBackendHostV1 *host,
            const QtMozBackendRuntimeCallbacksV1 *callbacks,
            QtMozBackendRuntimeHandle *runtime);
    QtMozBackendResult (*start)(QtMozBackendRuntimeHandle runtime);
    QtMozBackendResult (*stop)(QtMozBackendRuntimeHandle runtime);
    QtMozBackendResult (*destroy)(QtMozBackendRuntimeHandle runtime);
} QtMozBackendRuntimeOpsV1;

typedef struct QtMozBackendApiV1 {
    uint32_t struct_size;
    uint32_t abi_major;
    uint32_t abi_minor;
    uint64_t capabilities;

    /* Optional diagnostic name. It is not used for compatibility decisions. */
    const char *name;

    /* Optional v1.1 operations, advertised with CAP_RUNTIME_LIFECYCLE_V1. */
    const QtMozBackendRuntimeOpsV1 *runtime_ops;
} QtMozBackendApiV1;

/* API strings and nested tables remain valid until the backend DSO unloads. */

#define QTMOZ_BACKEND_HOST_V1_REQUIRED_SIZE \
    (offsetof(QtMozBackendHostV1, capabilities) + sizeof(uint64_t))
#define QTMOZ_BACKEND_HOST_V1_LOG_SIZE \
    (offsetof(QtMozBackendHostV1, log) + sizeof(QtMozBackendLogV1))
#define QTMOZ_BACKEND_RUNTIME_CALLBACKS_V1_REQUIRED_SIZE \
    (offsetof(QtMozBackendRuntimeCallbacksV1, destroyed) \
     + sizeof(((QtMozBackendRuntimeCallbacksV1 *)0)->destroyed))
#define QTMOZ_BACKEND_RUNTIME_OPS_V1_REQUIRED_SIZE \
    (offsetof(QtMozBackendRuntimeOpsV1, destroy) \
     + sizeof(((QtMozBackendRuntimeOpsV1 *)0)->destroy))
#define QTMOZ_BACKEND_API_V1_REQUIRED_SIZE \
    (offsetof(QtMozBackendApiV1, capabilities) + sizeof(uint64_t))
#define QTMOZ_BACKEND_API_V1_NAME_SIZE \
    (offsetof(QtMozBackendApiV1, name) \
     + sizeof(((QtMozBackendApiV1 *)0)->name))
#define QTMOZ_BACKEND_API_V1_RUNTIME_SIZE \
    (offsetof(QtMozBackendApiV1, runtime_ops) \
     + sizeof(((QtMozBackendApiV1 *)0)->runtime_ops))

typedef QtMozBackendResult (*QtMozBackendQueryV1)(
        const QtMozBackendHostV1 *host, QtMozBackendApiV1 *api);

/*
 * This symbol belongs to a backend DSO, not to libqt5embedwidget.
 *
 * Before calling it, the host sets api->struct_size to the writable capacity
 * of the output table. If that capacity is below API_V1_REQUIRED_SIZE, the
 * backend returns BUFFER_TOO_SMALL without modifying the table. Otherwise it
 * does not write beyond the capacity and replaces struct_size with the number
 * of bytes it provided. api->abi_minor reports backend support; consumers use
 * the lower host/API minor together with struct_size when reading extensions.
 */
QTMOZ_BACKEND_EXPORT QtMozBackendResult qtmoz_backend_query_v1(
        const QtMozBackendHostV1 *host, QtMozBackendApiV1 *api);

#ifdef __cplusplus
}
#endif

#undef QTMOZ_BACKEND_EXPORT

#endif // QTMOZ_BACKEND_ABI_H

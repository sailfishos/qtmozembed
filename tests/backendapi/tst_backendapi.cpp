/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "runtime/backendapi_p.h"

#include <cstdio>
#include <cstring>

namespace {

extern "C" int qtmoz_backend_c_abi_test(void);

int failures = 0;

void verify(bool condition, const char *expression, int line)
{
    if (!condition) {
        std::fprintf(stderr, "line %d: verification failed: %s\n",
                     line, expression);
        ++failures;
    }
}

#define VERIFY(expression) verify((expression), #expression, __LINE__)

QtMozBackendApiV1 descriptor()
{
    QtMozBackendApiV1 api = {
        sizeof(QtMozBackendApiV1),
        QTMOZ_BACKEND_ABI_MAJOR,
        QTMOZ_BACKEND_ABI_MINOR,
        QTMOZ_BACKEND_CAP_NONE,
        "test",
        nullptr
    };
    return api;
}

QtMozBackendHostV1 hostDescriptor()
{
    QtMozBackendHostV1 host = {
        sizeof(QtMozBackendHostV1),
        QTMOZ_BACKEND_ABI_MAJOR,
        QTMOZ_BACKEND_ABI_MINOR,
        QTMOZ_BACKEND_HOST_CAP_NONE,
        nullptr,
        nullptr
    };
    return host;
}

QtMozBackendResult createRuntime(
        const QtMozBackendHostV1 *,
        const QtMozBackendRuntimeCallbacksV1 *,
        QtMozBackendRuntimeHandle *runtime)
{
    if (runtime) {
        *runtime = 1;
    }
    return QTMOZ_BACKEND_RESULT_OK;
}

QtMozBackendResult changeRuntimeState(QtMozBackendRuntimeHandle)
{
    return QTMOZ_BACKEND_RESULT_OK;
}

void logMessage(void *, uint32_t, const QtMozBackendStringViewV1 *)
{
}

void runtimeStateChanged(void *)
{
}

QtMozBackendRuntimeOpsV1 runtimeOps()
{
    QtMozBackendRuntimeOpsV1 ops = {
        sizeof(QtMozBackendRuntimeOpsV1),
        QTMOZ_BACKEND_ABI_MAJOR,
        QTMOZ_BACKEND_ABI_MINOR,
        createRuntime,
        changeRuntimeState,
        changeRuntimeState,
        changeRuntimeState
    };
    return ops;
}

void testNullDescriptor()
{
    const QtMoz::BackendApiValidation validation =
            QtMoz::validateBackendApi(nullptr);
    VERIFY(!validation.isValid());
    VERIFY(validation.error == QtMoz::BackendApiError::NullDescriptor);
}

void testTruncatedDescriptor()
{
    QtMozBackendApiV1 api = descriptor();
    api.struct_size = QTMOZ_BACKEND_API_V1_REQUIRED_SIZE - 1;

    const QtMoz::BackendApiValidation validation =
            QtMoz::validateBackendApi(&api);
    VERIFY(!validation.isValid());
    VERIFY(validation.error == QtMoz::BackendApiError::TruncatedDescriptor);
    VERIFY(std::strcmp(QtMoz::backendApiName(&api), "") == 0);
}

void testIncompatibleMajor()
{
    QtMozBackendApiV1 api = descriptor();
    ++api.abi_major;

    const QtMoz::BackendApiValidation validation =
            QtMoz::validateBackendApi(&api);
    VERIFY(!validation.isValid());
    VERIFY(validation.error == QtMoz::BackendApiError::IncompatibleMajor);
}

void testHostDescriptor()
{
    VERIFY(QtMoz::validateBackendHost(nullptr).error
           == QtMoz::BackendApiError::NullDescriptor);

    QtMozBackendHostV1 host = hostDescriptor();
    VERIFY(QtMoz::validateBackendHost(&host).isValid());

    host.struct_size = QTMOZ_BACKEND_HOST_V1_REQUIRED_SIZE - 1;
    VERIFY(QtMoz::validateBackendHost(&host).error
           == QtMoz::BackendApiError::TruncatedDescriptor);

    host.struct_size = QTMOZ_BACKEND_HOST_V1_REQUIRED_SIZE;
    host.abi_minor = 0;
    VERIFY(QtMoz::validateBackendHost(&host).isValid());

    host.struct_size = sizeof(host);
    host.abi_minor = QTMOZ_BACKEND_ABI_MINOR;
    host.capabilities = QTMOZ_BACKEND_HOST_CAP_LOG_V1;
    VERIFY(QtMoz::validateBackendHost(&host).error
           == QtMoz::BackendApiError::IncompleteCapabilities);

    host.log = logMessage;
    VERIFY(QtMoz::validateBackendHost(&host).isValid());
}

void testCompatibleExtension()
{
    struct ExtendedDescriptor {
        QtMozBackendApiV1 api;
        uint8_t extension[32];
    } extended = { descriptor(), { 0 } };

    extended.api.struct_size = sizeof(extended);
    extended.api.abi_minor += 3;
    extended.api.capabilities = UINT64_C(1) << 63;

    const QtMoz::BackendApiValidation validation =
            QtMoz::validateBackendApi(&extended.api);
    VERIFY(validation.isValid());
    VERIFY(validation.negotiatedMinor == QTMOZ_BACKEND_ABI_MINOR);
    VERIFY(validation.capabilities == extended.api.capabilities);
    VERIFY(std::strcmp(QtMoz::backendApiName(&extended.api), "test") == 0);
}

void testRequiredCapabilities()
{
    QtMozBackendApiV1 api = descriptor();
    api.capabilities = QTMOZ_BACKEND_CAP_WINDOWLESS_SESSIONS_V1;

    const QtMoz::BackendApiValidation missing = QtMoz::validateBackendApi(
            &api, QTMOZ_BACKEND_CAP_EXTERNAL_EGL_IMAGE_LEASE_V1);
    VERIFY(!missing.isValid());
    VERIFY(missing.error == QtMoz::BackendApiError::MissingCapabilities);

    const QtMoz::BackendApiValidation present = QtMoz::validateBackendApi(
            &api, QTMOZ_BACKEND_CAP_WINDOWLESS_SESSIONS_V1);
    VERIFY(present.isValid());
}

void testRuntimeOperations()
{
    QtMozBackendApiV1 api = descriptor();
    QtMozBackendRuntimeOpsV1 ops = runtimeOps();
    api.capabilities = QTMOZ_BACKEND_CAP_RUNTIME_LIFECYCLE_V1;

    VERIFY(QtMoz::validateBackendApi(&api).error
           == QtMoz::BackendApiError::IncompleteCapabilities);

    api.runtime_ops = &ops;
    VERIFY(QtMoz::validateBackendApi(&api).isValid());
    VERIFY(QtMoz::backendRuntimeOps(&api) == &ops);

    ops.stop = nullptr;
    VERIFY(QtMoz::validateBackendApi(&api).error
           == QtMoz::BackendApiError::IncompleteCapabilities);

    ops = runtimeOps();
    ops.struct_size = QTMOZ_BACKEND_RUNTIME_OPS_V1_REQUIRED_SIZE - 1;
    VERIFY(QtMoz::validateBackendApi(&api).error
           == QtMoz::BackendApiError::IncompleteCapabilities);

    ops = runtimeOps();
    ops.abi_minor = 0;
    VERIFY(QtMoz::validateBackendApi(&api).error
           == QtMoz::BackendApiError::IncompleteCapabilities);

    ops = runtimeOps();
    api.abi_minor = 0;
    VERIFY(QtMoz::validateBackendApi(&api).error
           == QtMoz::BackendApiError::IncompleteCapabilities);
}

void testRuntimeCallbacks()
{
    VERIFY(QtMoz::validateBackendRuntimeCallbacks(nullptr).error
           == QtMoz::BackendApiError::NullDescriptor);

    QtMozBackendRuntimeCallbacksV1 callbacks = {
        sizeof(QtMozBackendRuntimeCallbacksV1),
        QTMOZ_BACKEND_ABI_MAJOR,
        QTMOZ_BACKEND_ABI_MINOR,
        nullptr,
        runtimeStateChanged,
        runtimeStateChanged
    };

    VERIFY(QtMoz::validateBackendRuntimeCallbacks(&callbacks).isValid());

    callbacks.struct_size =
            QTMOZ_BACKEND_RUNTIME_CALLBACKS_V1_REQUIRED_SIZE - 1;
    VERIFY(QtMoz::validateBackendRuntimeCallbacks(&callbacks).error
           == QtMoz::BackendApiError::TruncatedDescriptor);

    callbacks.struct_size = sizeof(callbacks);
    callbacks.initialized = nullptr;
    VERIFY(QtMoz::validateBackendRuntimeCallbacks(&callbacks).error
           == QtMoz::BackendApiError::IncompleteCapabilities);

    callbacks.initialized = runtimeStateChanged;
    callbacks.abi_minor = 0;
    VERIFY(QtMoz::validateBackendRuntimeCallbacks(&callbacks).error
           == QtMoz::BackendApiError::IncompleteCapabilities);

    callbacks.abi_minor = QTMOZ_BACKEND_ABI_MINOR;
    callbacks.destroyed = nullptr;
    VERIFY(QtMoz::validateBackendRuntimeCallbacks(&callbacks).error
           == QtMoz::BackendApiError::IncompleteCapabilities);
}

void testBoundedQuery()
{
    struct ExtendedDescriptor {
        QtMozBackendApiV1 api;
        uint8_t canary[16];
    } extended = { descriptor(), { 0xa5 } };
    QtMozBackendHostV1 host = hostDescriptor();
    QtMozBackendApiV1 api = descriptor();
    const QtMozBackendRuntimeOpsV1 * const untouched =
            reinterpret_cast<const QtMozBackendRuntimeOpsV1 *>(UINTPTR_MAX);
    api.struct_size = QTMOZ_BACKEND_API_V1_NAME_SIZE;
    api.runtime_ops = untouched;

    VERIFY(QtMoz::queryEmbedLiteBackendV1(&host, &api)
           == QTMOZ_BACKEND_RESULT_OK);
    VERIFY(api.struct_size == QTMOZ_BACKEND_API_V1_NAME_SIZE);
    VERIFY(api.runtime_ops == untouched);
    VERIFY(QtMoz::validateBackendApi(&api).isValid());
    VERIFY(QtMoz::backendRuntimeOps(&api) == nullptr);

    extended.api.struct_size = sizeof(extended);
    VERIFY(QtMoz::queryEmbedLiteBackendV1(&host, &extended.api)
           == QTMOZ_BACKEND_RESULT_OK);
    VERIFY(extended.api.struct_size == sizeof(QtMozBackendApiV1));
    for (size_t i = 0; i < sizeof(extended.canary); ++i) {
        VERIFY(extended.canary[i] == (i == 0 ? 0xa5 : 0));
    }

    api = descriptor();
    api.struct_size = sizeof(api.struct_size);
    VERIFY(QtMoz::queryEmbedLiteBackendV1(&host, &api)
           == QTMOZ_BACKEND_RESULT_BUFFER_TOO_SMALL);
    VERIFY(api.struct_size == sizeof(api.struct_size));

    host = hostDescriptor();
    ++host.abi_major;
    api = descriptor();
    VERIFY(QtMoz::queryEmbedLiteBackendV1(&host, &api)
           == QTMOZ_BACKEND_RESULT_INCOMPATIBLE_ABI);

    host = hostDescriptor();
    host.capabilities = QTMOZ_BACKEND_HOST_CAP_LOG_V1;
    host.struct_size = QTMOZ_BACKEND_HOST_V1_REQUIRED_SIZE;
    api = descriptor();
    VERIFY(QtMoz::queryEmbedLiteBackendV1(&host, &api)
           == QTMOZ_BACKEND_RESULT_INVALID_ARGUMENT);
}

void testBuiltInDescriptor()
{
    const QtMozBackendApiV1 &api = QtMoz::embedLiteBackendApiV1();
    const QtMoz::BackendApiValidation validation =
            QtMoz::validateBackendApi(&api);

    VERIFY(validation.isValid());
    VERIFY(validation.capabilities == QTMOZ_BACKEND_CAP_NONE);
    VERIFY(std::strcmp(QtMoz::backendApiName(&api), "embedlite") == 0);
}

} // namespace

int main()
{
    VERIFY(qtmoz_backend_c_abi_test() > 0);
    testNullDescriptor();
    testTruncatedDescriptor();
    testIncompatibleMajor();
    testHostDescriptor();
    testCompatibleExtension();
    testRequiredCapabilities();
    testRuntimeOperations();
    testRuntimeCallbacks();
    testBoundedQuery();
    testBuiltInDescriptor();
    return failures == 0 ? 0 : 1;
}

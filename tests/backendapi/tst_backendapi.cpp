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
        "test"
    };
    return api;
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
    testCompatibleExtension();
    testRequiredCapabilities();
    testBuiltInDescriptor();
    return failures == 0 ? 0 : 1;
}

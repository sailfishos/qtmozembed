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
static_assert(std::is_standard_layout<QtMozBackendRuntimeCallbacksV1>::value,
              "Runtime callbacks must have a C-compatible layout");
static_assert(std::is_standard_layout<QtMozBackendRuntimeOpsV1>::value,
              "Runtime operations must have a C-compatible layout");
static_assert(offsetof(QtMozBackendHostV1, struct_size) == 0,
              "The backend host table size must be its first field");
static_assert(offsetof(QtMozBackendApiV1, struct_size) == 0,
              "The backend API table size must be its first field");
static_assert(offsetof(QtMozBackendRuntimeCallbacksV1, struct_size) == 0,
              "The callback table size must be its first field");
static_assert(offsetof(QtMozBackendRuntimeOpsV1, struct_size) == 0,
              "The runtime table size must be its first field");
static_assert(QTMOZ_BACKEND_HOST_V1_REQUIRED_SIZE
                      <= sizeof(QtMozBackendHostV1),
              "The required host prefix must fit in its table");
static_assert(QTMOZ_BACKEND_API_V1_REQUIRED_SIZE
                      <= sizeof(QtMozBackendApiV1),
              "The required API prefix must fit in its table");
static_assert(QTMOZ_BACKEND_RUNTIME_CALLBACKS_V1_REQUIRED_SIZE
                      <= sizeof(QtMozBackendRuntimeCallbacksV1),
              "The required callback prefix must fit in its table");
static_assert(QTMOZ_BACKEND_RUNTIME_OPS_V1_REQUIRED_SIZE
                      <= sizeof(QtMozBackendRuntimeOpsV1),
              "The required runtime prefix must fit in its table");
static_assert(sizeof(QtMozBackendApiV1) <= UINT32_MAX,
              "The backend API table size must fit in struct_size");

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
    if (api->capabilities & QTMOZ_BACKEND_CAP_RUNTIME_LIFECYCLE_V1) {
        const QtMozBackendRuntimeOpsV1 * const ops = backendRuntimeOps(api);
        if (api->abi_minor < 1 || !ops
                || ops->struct_size
                        < QTMOZ_BACKEND_RUNTIME_OPS_V1_REQUIRED_SIZE
                || ops->abi_major != QTMOZ_BACKEND_ABI_MAJOR
                || ops->abi_minor < 1
                || !ops->create || !ops->start || !ops->stop || !ops->destroy) {
            result.error = BackendApiError::IncompleteCapabilities;
            return result;
        }
    }

    result.error = BackendApiError::None;
    return result;
}

BackendApiValidation validateBackendHost(const QtMozBackendHostV1 *host,
                                         uint64_t requiredCapabilities)
{
    BackendApiValidation result = {
        BackendApiError::NullDescriptor,
        0,
        QTMOZ_BACKEND_HOST_CAP_NONE
    };

    if (!host) {
        return result;
    }
    if (host->struct_size < QTMOZ_BACKEND_HOST_V1_REQUIRED_SIZE) {
        result.error = BackendApiError::TruncatedDescriptor;
        return result;
    }
    if (host->abi_major != QTMOZ_BACKEND_ABI_MAJOR) {
        result.error = BackendApiError::IncompatibleMajor;
        return result;
    }

    result.negotiatedMinor = QTMOZ_BACKEND_ABI_MINOR < host->abi_minor
            ? QTMOZ_BACKEND_ABI_MINOR : host->abi_minor;
    result.capabilities = host->capabilities;
    if ((host->capabilities & requiredCapabilities) != requiredCapabilities) {
        result.error = BackendApiError::MissingCapabilities;
        return result;
    }
    if (host->capabilities & QTMOZ_BACKEND_HOST_CAP_LOG_V1) {
        if (host->abi_minor < 1
                || host->struct_size < QTMOZ_BACKEND_HOST_V1_LOG_SIZE
                || !host->log) {
            result.error = BackendApiError::IncompleteCapabilities;
            return result;
        }
    }

    result.error = BackendApiError::None;
    return result;
}

BackendApiValidation validateBackendRuntimeCallbacks(
        const QtMozBackendRuntimeCallbacksV1 *callbacks)
{
    BackendApiValidation result = {
        BackendApiError::NullDescriptor,
        0,
        QTMOZ_BACKEND_CAP_NONE
    };

    if (!callbacks) {
        return result;
    }
    if (callbacks->struct_size
            < QTMOZ_BACKEND_RUNTIME_CALLBACKS_V1_REQUIRED_SIZE) {
        result.error = BackendApiError::TruncatedDescriptor;
        return result;
    }
    if (callbacks->abi_major != QTMOZ_BACKEND_ABI_MAJOR) {
        result.error = BackendApiError::IncompatibleMajor;
        return result;
    }
    if (callbacks->abi_minor < 1
            || !callbacks->initialized || !callbacks->destroyed) {
        result.error = BackendApiError::IncompleteCapabilities;
        return result;
    }

    result.negotiatedMinor = QTMOZ_BACKEND_ABI_MINOR < callbacks->abi_minor
            ? QTMOZ_BACKEND_ABI_MINOR : callbacks->abi_minor;
    result.error = BackendApiError::None;
    return result;
}

const char *backendApiName(const QtMozBackendApiV1 *api)
{
    return api && api->struct_size >= QTMOZ_BACKEND_API_V1_NAME_SIZE
            && api->name
            ? api->name : "";
}

const QtMozBackendRuntimeOpsV1 *backendRuntimeOps(
        const QtMozBackendApiV1 *api)
{
    return api && api->struct_size >= QTMOZ_BACKEND_API_V1_RUNTIME_SIZE
            ? api->runtime_ops : nullptr;
}

} // namespace QtMoz

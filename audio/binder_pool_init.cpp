/*
 * Copyright (C) 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <android/binder_process.h>
#include <binder/ProcessState.h>
#include <log/log.h>

// The upstream legacy entry starts the pool before the NDK API sets its size.
// Configure it before main, retaining upstream service registration unchanged.
__attribute__((constructor)) static void configureVendorBinderPool() {
    ::android::ProcessState::initWithDriver("/dev/vndbinder");
    LOG_ALWAYS_FATAL_IF(!ABinderProcess_setThreadPoolMaxThreadCount(1),
                        "Could not configure the legacy audio Binder pool");
}

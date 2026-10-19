/*
 * Copyright (C) 2020 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include <log/log.h>
#include <utils/String8.h>

using audio_error_callback = void (*)(int);
using add_error_callback = uintptr_t (*)(audio_error_callback);

static void* audio_system_symbol(const char* symbol) {
    // Android 17 moved AudioSystem into libaudiobase. Resolve through the
    // explicitly shared libaudioclient handle and its system dependencies;
    // RTLD_DEFAULT in the vendor namespace cannot see that dependency group.
    static void* client = dlopen("libaudioclient.so", RTLD_NOW | RTLD_LOCAL);
    if (client == nullptr) {
        ALOGE("Qualcomm RIL AudioSystem: cannot load libaudioclient: %s", dlerror());
        return nullptr;
    }
    void* address = dlsym(client, symbol);
    if (address == nullptr) {
        ALOGE("Qualcomm RIL AudioSystem: cannot resolve %s: %s", symbol, dlerror());
    }
    return address;
}

extern "C" void _ZN7android11AudioSystem16setErrorCallbackEPFviE(
        audio_error_callback cb) {
    static auto addErrorCallback = reinterpret_cast<add_error_callback>(
            audio_system_symbol("_ZN7android11AudioSystem16addErrorCallbackEPFviE"));

    if (addErrorCallback != nullptr) {
        addErrorCallback(cb);
    }
}

int legacy_set_parameters(int io, const android::String8& pairs)
        asm("_ZN7android11AudioSystem13setParametersEiRKNS_7String8E");
int legacy_set_parameters(int io, const android::String8& pairs) {
    using function = int (*)(int, const android::String8&);
    static auto set_parameters = reinterpret_cast<function>(
            audio_system_symbol("_ZN7android11AudioSystem13setParametersEiRKNS_7String8E"));
    return set_parameters != nullptr ? set_parameters(io, pairs) : -ENOSYS;
}

android::String8 legacy_get_parameters(int io, const android::String8& keys)
        asm("_ZN7android11AudioSystem13getParametersEiRKNS_7String8E");
android::String8 legacy_get_parameters(int io, const android::String8& keys) {
    using function = android::String8 (*)(int, const android::String8&);
    static auto get_parameters = reinterpret_cast<function>(
            audio_system_symbol("_ZN7android11AudioSystem13getParametersEiRKNS_7String8E"));
    return get_parameters != nullptr ? get_parameters(io, keys) : android::String8();
}

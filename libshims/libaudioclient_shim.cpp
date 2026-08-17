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

#include <android/dlext.h>
#include <dlfcn.h>
#include <stdint.h>
#include <utils/Errors.h>
#include <utils/String8.h>

namespace {

void* getAudioClientHandle() {
    static void* handle = []() {
        using GetExportedNamespace = android_namespace_t* (*)(const char*);
        auto getExportedNamespace = reinterpret_cast<GetExportedNamespace>(
                dlsym(RTLD_DEFAULT, "__loader_android_get_exported_namespace"));

        android_dlextinfo info = {};
        info.flags = ANDROID_DLEXT_USE_NAMESPACE;
        info.library_namespace = getExportedNamespace != nullptr
                ? getExportedNamespace("system") : nullptr;
        if (info.library_namespace == nullptr) {
            return static_cast<void*>(nullptr);
        }
        return android_dlopen_ext("libaudioclient.so", RTLD_NOW, &info);
    }();
    return handle;
}

template <typename Function>
Function getAudioClientSymbol(const char* symbol) {
    void* handle = getAudioClientHandle();
    return handle == nullptr ? nullptr : reinterpret_cast<Function>(dlsym(handle, symbol));
}

}  // namespace

using audio_error_callback = void (*)(int);
using add_error_callback = uintptr_t (*)(audio_error_callback);
using get_parameters = android::String8 (*)(int, const android::String8&);
using set_parameters = android::status_t (*)(int, const android::String8&);

extern "C" void _ZN7android11AudioSystem16setErrorCallbackEPFviE(
        audio_error_callback cb) {
    static auto addErrorCallback = getAudioClientSymbol<add_error_callback>(
            "_ZN7android11AudioSystem16addErrorCallbackEPFviE");

    if (addErrorCallback != nullptr) {
        addErrorCallback(cb);
    }
}

extern "C" android::status_t _ZN7android11AudioSystem13setParametersEiRKNS_7String8E(
        int ioHandle, const android::String8& keyValuePairs) {
    static auto setParameters = getAudioClientSymbol<set_parameters>(
            "_ZN7android11AudioSystem13setParametersEiRKNS_7String8E");
    return setParameters == nullptr ? android::NO_ERROR : setParameters(ioHandle, keyValuePairs);
}

android::String8 legacyGetParameters(int ioHandle, const android::String8& keys)
        __asm__("_ZN7android11AudioSystem13getParametersEiRKNS_7String8E");

android::String8 legacyGetParameters(int ioHandle, const android::String8& keys) {
    static auto getParameters = getAudioClientSymbol<get_parameters>(
            "_ZN7android11AudioSystem13getParametersEiRKNS_7String8E");
    return getParameters == nullptr ? android::String8() : getParameters(ioHandle, keys);
}

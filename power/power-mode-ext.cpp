/*
 * Copyright (C) 2026 The LineageOS Project
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

#include <aidl/android/hardware/power/Mode.h>

#include "power-common.h"

namespace aidl::android::hardware::power::impl {

bool isDeviceSpecificModeSupported(Mode type, bool* isSupported) {
    if (type != Mode::CAMERA_STREAMING_HIGH) {
        return false;
    }

    *isSupported = true;
    return true;
}

bool setDeviceSpecificMode(Mode type, bool enabled) {
    if (type != Mode::CAMERA_STREAMING_HIGH) {
        return false;
    }

    char metadata[] = "state=0";
    metadata[6] = enabled ? '1' : '0';
    power_hint(POWER_HINT_VIDEO_ENCODE, metadata);
    return true;
}

}  // namespace aidl::android::hardware::power::impl

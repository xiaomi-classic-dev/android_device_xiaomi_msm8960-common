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

#define LOG_TAG "LightService"

#include "Light.h"

#include <android-base/file.h>
#include <log/log.h>

#include <array>
#include <string>
#include <vector>
#include <unistd.h>

namespace aidl::android::hardware::light::implementation {
namespace {

constexpr const char* kLcdBacklight = "/sys/class/leds/lcd-backlight/brightness";

constexpr std::array<const char*, 2> kRedBrightness = {
        "/sys/class/leds/red/brightness",
        "/sys/class/leds/led:red/brightness",
};
constexpr std::array<const char*, 2> kGreenBrightness = {
        "/sys/class/leds/green/brightness",
        "/sys/class/leds/led:green/brightness",
};
constexpr std::array<const char*, 2> kBlueBrightness = {
        "/sys/class/leds/blue/brightness",
        "/sys/class/leds/led:blue/brightness",
};
constexpr std::array<const char*, 1> kWhiteBrightness = {
        "/sys/class/leds/white/brightness",
};

template <size_t N>
bool writeAny(const std::array<const char*, N>& paths, int value) {
    const std::string data = std::to_string(value) + "\n";

    for (const char* path : paths) {
        if (::android::base::WriteStringToFile(data, path)) {
            return true;
        }
    }

    ALOGV("No writable LED node for value %d", value);
    return false;
}

bool writeValue(const char* path, int value) {
    if (!::android::base::WriteStringToFile(std::to_string(value) + "\n", path)) {
        ALOGV("Failed to write %d to %s", value, path);
        return false;
    }
    return true;
}

template <size_t N>
bool nodeExists(const std::array<const char*, N>& paths) {
    for (const char* path : paths) {
        if (!access(path, F_OK)) {
            return true;
        }
    }
    return false;
}

uint32_t rgbToBrightness(const LightState& state) {
    const uint32_t color = state.color & 0x00ffffff;
    return (77 * ((color >> 16) & 0xff) + 150 * ((color >> 8) & 0xff) +
            29 * (color & 0xff)) >>
           8;
}

bool isLit(const LightState& state) {
    return state.color & 0x00ffffff;
}

void setBacklight(const LightState& state) {
    writeValue(kLcdBacklight, rgbToBrightness(state));
}

void setNotificationLed(const LightState& state) {
    const bool rgb = nodeExists(kRedBrightness) && nodeExists(kGreenBrightness) &&
                     nodeExists(kBlueBrightness);
    const uint32_t color = state.color;

    if (rgb) {
        writeAny(kRedBrightness, (color >> 16) & 0xff);
        writeAny(kGreenBrightness, (color >> 8) & 0xff);
        writeAny(kBlueBrightness, color & 0xff);
    } else {
        writeAny(kWhiteBrightness, rgbToBrightness(state));
    }
}

const LightState kOffState = {};
LightState gAttention;
LightState gNotification;
LightState gBattery;

void updateNotificationLed() {
    if (isLit(gBattery)) {
        setNotificationLed(gBattery);
    } else if (isLit(gAttention)) {
        setNotificationLed(gAttention);
    } else if (isLit(gNotification)) {
        setNotificationLed(gNotification);
    } else {
        setNotificationLed(kOffState);
    }
}

}  // namespace

ndk::ScopedAStatus Light::setLightState(int32_t id, const HwLightState& state) {
    const Type type = static_cast<Type>(id);
    std::lock_guard<std::mutex> lock(mLock);

    switch (type) {
        case Type::BACKLIGHT:
            setBacklight(state);
            return ndk::ScopedAStatus::ok();
        case Type::ATTENTION:
            gAttention = state;
            updateNotificationLed();
            return ndk::ScopedAStatus::ok();
        case Type::NOTIFICATIONS:
            gNotification = state;
            updateNotificationLed();
            return ndk::ScopedAStatus::ok();
        case Type::BATTERY:
            gBattery = state;
            updateNotificationLed();
            return ndk::ScopedAStatus::ok();
        default:
            return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
}

ndk::ScopedAStatus Light::getLights(std::vector<HwLight>* lights) {
    lights->clear();
    for (const Type type : {Type::BACKLIGHT, Type::ATTENTION,
                            Type::NOTIFICATIONS, Type::BATTERY}) {
        HwLight light;
        light.id = static_cast<int32_t>(type);
        light.ordinal = 0;
        light.type = type;
        lights->push_back(light);
    }
    return ndk::ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::light::implementation

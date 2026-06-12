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
#include <android-base/strings.h>
#include <log/log.h>

#include <array>
#include <cstdlib>
#include <string>
#include <vector>
#include <unistd.h>

namespace android {
namespace hardware {
namespace light {
namespace V2_0 {
namespace implementation {
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

constexpr std::array<const char*, 2> kUsbOnline = {
        "/sys/class/power_supply/usb/online",
        "/sys/class/power_supply/ac/online",
};
constexpr std::array<const char*, 1> kDcOnline = {
        "/sys/class/power_supply/pm8921-dc/online",
};
constexpr const char* kBatteryStatus = "/sys/class/power_supply/battery/status";
constexpr const char* kBatteryCapacity = "/sys/class/power_supply/battery/capacity";
constexpr uint32_t kBatteryFullColor = 0xff00ff00;

bool parseInt(const std::string& data, int* value) {
    const std::string trimmed = android::base::Trim(data);
    char* end = nullptr;
    const long parsed = std::strtol(trimmed.c_str(), &end, 10);

    if (trimmed.empty() || end == nullptr || *end != '\0') {
        return false;
    }

    *value = parsed;
    return true;
}

template <size_t N>
bool writeAny(const std::array<const char*, N>& paths, int value) {
    const std::string data = std::to_string(value) + "\n";

    for (const char* path : paths) {
        if (android::base::WriteStringToFile(data, path)) {
            return true;
        }
    }

    ALOGV("No writable LED node for value %d", value);
    return false;
}

bool writeValue(const char* path, int value) {
    if (!android::base::WriteStringToFile(std::to_string(value) + "\n", path)) {
        ALOGV("Failed to write %d to %s", value, path);
        return false;
    }
    return true;
}

template <size_t N>
bool readAnyInt(const std::array<const char*, N>& paths, int* value) {
    std::string data;

    for (const char* path : paths) {
        if (android::base::ReadFileToString(path, &data) && parseInt(data, value)) {
            return true;
        }
    }

    return false;
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

bool chargerOnline() {
    int online = 0;
    if (readAnyInt(kUsbOnline, &online) && online > 0) {
        return true;
    }
    return readAnyInt(kDcOnline, &online) && online > 0;
}

bool batteryFull() {
    std::string status;
    if (android::base::ReadFileToString(kBatteryStatus, &status) &&
        android::base::Trim(status) == "Full") {
        return true;
    }

    int capacity = 0;
    return android::base::ReadFileToString(kBatteryCapacity, &status) &&
           parseInt(status, &capacity) && capacity >= 100;
}

LightState normalizeBatteryState(const LightState& state) {
    if (isLit(state) || !chargerOnline() || !batteryFull()) {
        return state;
    }

    LightState fullState = state;
    fullState.color = kBatteryFullColor;
    fullState.flashMode = Flash::NONE;
    return fullState;
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
    const LightState battery = normalizeBatteryState(gBattery);

    if (isLit(battery)) {
        setNotificationLed(battery);
    } else if (isLit(gAttention)) {
        setNotificationLed(gAttention);
    } else if (isLit(gNotification)) {
        setNotificationLed(gNotification);
    } else {
        setNotificationLed(kOffState);
    }
}

}  // namespace

Return<Status> Light::setLight(Type type, const LightState& state) {
    std::lock_guard<std::mutex> lock(mLock);

    switch (type) {
        case Type::BACKLIGHT:
            setBacklight(state);
            return Status::SUCCESS;
        case Type::ATTENTION:
            gAttention = state;
            updateNotificationLed();
            return Status::SUCCESS;
        case Type::NOTIFICATIONS:
            gNotification = state;
            updateNotificationLed();
            return Status::SUCCESS;
        case Type::BATTERY:
            gBattery = state;
            updateNotificationLed();
            return Status::SUCCESS;
        default:
            return Status::LIGHT_NOT_SUPPORTED;
    }
}

Return<void> Light::getSupportedTypes(getSupportedTypes_cb hidl_cb) {
    hidl_cb(std::vector<Type>{
            Type::BACKLIGHT,
            Type::ATTENTION,
            Type::NOTIFICATIONS,
            Type::BATTERY,
    });
    return Void();
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace light
}  // namespace hardware
}  // namespace android

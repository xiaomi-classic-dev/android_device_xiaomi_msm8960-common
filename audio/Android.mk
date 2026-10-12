LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := android.hardware.audio.service.msm8960
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MULTILIB := 32
LOCAL_OVERRIDES_MODULES := android.hardware.audio.service
LOCAL_SRC_FILES := \
    ../../../../hardware/interfaces/audio/common/all-versions/default/service/service.cpp \
    binder_pool_init.cpp
LOCAL_INIT_RC := android.hardware.audio.service.msm8960.rc
LOCAL_CPP_STD := c++17
LOCAL_CFLAGS := -Wall -Wextra -Werror
LOCAL_SHARED_LIBRARIES := libcutils libbinder libbinder_ndk libhidlbase liblog libutils libhardware
include $(BUILD_EXECUTABLE)

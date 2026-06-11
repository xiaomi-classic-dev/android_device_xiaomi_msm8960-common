#ifndef ANDROID_HARDWARE_QCOM_CAMERA3_WRAPPER_H
#define ANDROID_HARDWARE_QCOM_CAMERA3_WRAPPER_H

#include <hardware/camera_common.h>
#include <hardware/hardware.h>

int camera3_get_camera_info(int camera_id, struct camera_info *info);
int camera3_device_open(const struct hw_module_t *module, const char *id,
        struct hw_device_t **device);

#endif

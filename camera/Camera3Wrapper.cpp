#define LOG_TAG "QCamera3Wrapper"

#include "Camera3Wrapper.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cutils/log.h>
#include <hardware/camera3.h>
#include <system/camera_metadata.h>

#include "QCameraHAL.h"

namespace {

struct Camera3WrapperDevice {
    camera3_device_t device;
    int camera_id;
    const camera3_callback_ops_t *callbacks;
};

camera_metadata_t *gStaticMetadata[MSM_MAX_CAMERA_SENSORS];
camera_metadata_t *gDefaultRequest;

int add_u8(camera_metadata_t *m, uint32_t tag, uint8_t value)
{
    return add_camera_metadata_entry(m, tag, &value, 1);
}

int add_i32(camera_metadata_t *m, uint32_t tag, int32_t value)
{
    return add_camera_metadata_entry(m, tag, &value, 1);
}

camera_metadata_t *get_static_metadata(int camera_id)
{
    if (camera_id < 0 || camera_id >= MSM_MAX_CAMERA_SENSORS)
        return NULL;

    if (gStaticMetadata[camera_id])
        return gStaticMetadata[camera_id];

    camera_metadata_t *m = allocate_camera_metadata(16, 64);
    if (!m)
        return NULL;

    uint8_t hardware_level = ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_LEGACY;
    uint8_t capabilities[] = {
        ANDROID_REQUEST_AVAILABLE_CAPABILITIES_BACKWARD_COMPATIBLE,
    };
    int32_t partial_count = 1;
    uint8_t ae_modes[] = {
        ANDROID_CONTROL_AE_MODE_ON,
    };
    uint8_t af_modes[] = {
        ANDROID_CONTROL_AF_MODE_AUTO,
    };
    uint8_t awb_modes[] = {
        ANDROID_CONTROL_AWB_MODE_AUTO,
    };
    int32_t jpeg_orientation[] = {0, 90, 180, 270};

    add_camera_metadata_entry(m, ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL,
            &hardware_level, 1);
    add_camera_metadata_entry(m, ANDROID_REQUEST_AVAILABLE_CAPABILITIES,
            capabilities, sizeof(capabilities) / sizeof(capabilities[0]));
    add_camera_metadata_entry(m, ANDROID_REQUEST_PARTIAL_RESULT_COUNT,
            &partial_count, 1);
    add_camera_metadata_entry(m, ANDROID_CONTROL_AE_AVAILABLE_MODES,
            ae_modes, sizeof(ae_modes) / sizeof(ae_modes[0]));
    add_camera_metadata_entry(m, ANDROID_CONTROL_AF_AVAILABLE_MODES,
            af_modes, sizeof(af_modes) / sizeof(af_modes[0]));
    add_camera_metadata_entry(m, ANDROID_CONTROL_AWB_AVAILABLE_MODES,
            awb_modes, sizeof(awb_modes) / sizeof(awb_modes[0]));
    add_camera_metadata_entry(m, ANDROID_JPEG_AVAILABLE_THUMBNAIL_SIZES,
            jpeg_orientation, sizeof(jpeg_orientation) / sizeof(jpeg_orientation[0]));

    gStaticMetadata[camera_id] = m;
    return m;
}

const camera_metadata_t *get_default_request()
{
    if (gDefaultRequest)
        return gDefaultRequest;

    camera_metadata_t *m = allocate_camera_metadata(8, 32);
    if (!m)
        return NULL;

    add_u8(m, ANDROID_CONTROL_MODE, ANDROID_CONTROL_MODE_AUTO);
    add_u8(m, ANDROID_CONTROL_AE_MODE, ANDROID_CONTROL_AE_MODE_ON);
    add_u8(m, ANDROID_CONTROL_AF_MODE, ANDROID_CONTROL_AF_MODE_AUTO);
    add_u8(m, ANDROID_CONTROL_AWB_MODE, ANDROID_CONTROL_AWB_MODE_AUTO);
    add_i32(m, ANDROID_REQUEST_ID, 0);

    gDefaultRequest = m;
    return gDefaultRequest;
}

Camera3WrapperDevice *to_wrapper(const camera3_device_t *device)
{
    return device ? static_cast<Camera3WrapperDevice *>(device->priv) : NULL;
}

int camera3_initialize(const camera3_device_t *device,
        const camera3_callback_ops_t *callback_ops)
{
    Camera3WrapperDevice *wrapper = to_wrapper(device);
    if (!wrapper || !callback_ops)
        return -EINVAL;

    wrapper->callbacks = callback_ops;
    return 0;
}

int camera3_configure_streams(const camera3_device_t *,
        camera3_stream_configuration_t *stream_list)
{
    if (!stream_list || stream_list->num_streams == 0 || !stream_list->streams)
        return -EINVAL;

    for (uint32_t i = 0; i < stream_list->num_streams; i++) {
        camera3_stream_t *stream = stream_list->streams[i];
        if (!stream)
            return -EINVAL;

        stream->usage |= GRALLOC_USAGE_HW_CAMERA_WRITE;
        stream->max_buffers = 1;
        stream->priv = stream;
    }

    return 0;
}

const camera_metadata_t *camera3_construct_default_request_settings(
        const camera3_device_t *, int)
{
    return get_default_request();
}

int camera3_process_capture_request(const camera3_device_t *,
        camera3_capture_request_t *)
{
    return -ENOSYS;
}

void camera3_dump(const camera3_device_t *, int fd)
{
    dprintf(fd, "msm8960 camera3 wrapper skeleton\n");
}

int camera3_flush(const camera3_device_t *)
{
    return 0;
}

int camera3_close(hw_device_t *hw_dev)
{
    camera3_device_t *device = reinterpret_cast<camera3_device_t *>(hw_dev);
    Camera3WrapperDevice *wrapper = to_wrapper(device);
    free(wrapper);
    return 0;
}

camera3_device_ops_t gCamera3Ops = {
    .initialize = camera3_initialize,
    .configure_streams = camera3_configure_streams,
    .register_stream_buffers = NULL,
    .construct_default_request_settings = camera3_construct_default_request_settings,
    .process_capture_request = camera3_process_capture_request,
    .get_metadata_vendor_tag_ops = NULL,
    .dump = camera3_dump,
    .flush = camera3_flush,
    .signal_stream_flush = NULL,
    .is_reconfiguration_required = NULL,
    .reserved = {0},
};

} // namespace

int camera3_get_camera_info(int camera_id, struct camera_info *info)
{
    if (!info)
        return -EINVAL;

    android::CameraInfo legacy_info;
    memset(&legacy_info, -1, sizeof(legacy_info));
    android::HAL_getCameraInfo(camera_id, &legacy_info);
    if (legacy_info.facing < 0)
        return -EINVAL;

    memset(info, 0, sizeof(*info));
    info->facing = legacy_info.facing;
    info->orientation = legacy_info.orientation;
    info->device_version = CAMERA_DEVICE_API_VERSION_3_2;
    info->static_camera_characteristics = get_static_metadata(camera_id);
    info->resource_cost = 100;
    info->conflicting_devices = NULL;
    info->conflicting_devices_length = 0;

    return info->static_camera_characteristics ? 0 : -ENOMEM;
}

int camera3_device_open(const struct hw_module_t *module, const char *id,
        struct hw_device_t **hw_device)
{
    if (!module || !id || !hw_device)
        return -EINVAL;

    Camera3WrapperDevice *wrapper =
            static_cast<Camera3WrapperDevice *>(calloc(1, sizeof(*wrapper)));
    if (!wrapper)
        return -ENOMEM;

    wrapper->camera_id = atoi(id);
    wrapper->device.common.tag = HARDWARE_DEVICE_TAG;
    wrapper->device.common.version = CAMERA_DEVICE_API_VERSION_3_2;
    wrapper->device.common.module = const_cast<hw_module_t *>(module);
    wrapper->device.common.close = camera3_close;
    wrapper->device.ops = &gCamera3Ops;
    wrapper->device.priv = wrapper;

    *hw_device = &wrapper->device.common;
    ALOGI("Opened camera %d as HAL3 wrapper skeleton", wrapper->camera_id);
    return 0;
}

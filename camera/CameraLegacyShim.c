#define _GNU_SOURCE

#include <android/log.h>
#include <dlfcn.h>
#include <poll.h>

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define LOG_TAG "CameraLegacyShim"

/*
 * This liboemcamera build checks newly-created pollfd slots after poll()
 * returns, even when those slots were not included in nfds. Clear the stale
 * stack values at the mctl_pp_poll_fn return site before they can trigger a
 * blocking read from an empty C2D pipe.
 */
#define MCTL_PP_POLL_RETURN_OFFSET 0x44b40U
#define MCTL_PP_MAX_POLL_FDS 4U

typedef int (*poll_function_t)(struct pollfd*, nfds_t, int);

static poll_function_t real_poll;

__attribute__((constructor)) static void camera_legacy_shim_init(void) {
    real_poll = (poll_function_t)dlsym(RTLD_NEXT, "poll");
}

static int is_mctl_pp_poll_call(void* return_address) {
    Dl_info info;
    uintptr_t caller;
    uintptr_t base;

    if (dladdr(return_address, &info) == 0 || info.dli_fbase == NULL ||
        info.dli_fname == NULL ||
        strstr(info.dli_fname, "/liboemcamera.so") == NULL) {
        return 0;
    }

    caller = (uintptr_t)return_address & ~(uintptr_t)1;
    base = (uintptr_t)info.dli_fbase;
    return caller >= base && caller - base == MCTL_PP_POLL_RETURN_OFFSET;
}

int poll(struct pollfd* fds, nfds_t nfds, int timeout) {
    void* return_address = __builtin_return_address(0);
    int rc;

    if (real_poll == NULL) {
        errno = ENOSYS;
        return -1;
    }

    rc = real_poll(fds, nfds, timeout);
    if (rc > 0 && fds != NULL && nfds >= 2 &&
        nfds < MCTL_PP_MAX_POLL_FDS && is_mctl_pp_poll_call(return_address)) {
        nfds_t i;

        for (i = nfds; i < MCTL_PP_MAX_POLL_FDS; ++i) {
            if (fds[i].revents != 0) {
                __android_log_print(ANDROID_LOG_WARN, LOG_TAG,
                                    "Clearing stale mctl PP revents[%u]=0x%x",
                                    (unsigned)i, (unsigned)(uint16_t)fds[i].revents);
            }
            fds[i].revents = 0;
        }
    }

    return rc;
}

int sensor1_open(void** handle, void* callback, intptr_t callback_data) {
    if (handle != NULL) {
        *handle = NULL;
    }

    (void)callback;
    (void)callback_data;
    __android_log_print(ANDROID_LOG_WARN, LOG_TAG,
                        "Disabling unsafe legacy camera DSPS client");
    return 1;
}

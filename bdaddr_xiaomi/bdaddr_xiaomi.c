/*
 * Copyright (C) 2016 The CyanogenMod Project
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

#define LOG_TAG "bdaddr_xiaomi"
#define LOG_NDEBUG 0

#include <cutils/log.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAC_ADDR_SIZE 6
#define BD_ADDR_FILE "/data/misc/bluetooth/bdaddr"
#define NV_READ_TIMEOUT_MS 10000

extern int qmi_nv_read_bd_addr(char** mac);

static bool is_valid_address(const unsigned char* address)
{
    unsigned char value = 0;
    int i;

    for (i = 0; i < MAC_ADDR_SIZE; i++) {
        value |= address[i];
    }
    return value != 0;
}

static bool read_existing_address(unsigned char* address)
{
    FILE *fp;
    unsigned int parsed[MAC_ADDR_SIZE];
    int i;

    fp = fopen(BD_ADDR_FILE, "r");
    if (fp == NULL) {
        return false;
    }
    if (fscanf(fp, "%02x:%02x:%02x:%02x:%02x:%02x",
            &parsed[0], &parsed[1], &parsed[2], &parsed[3], &parsed[4], &parsed[5])
            != MAC_ADDR_SIZE) {
        fclose(fp);
        return false;
    }
    fclose(fp);

    for (i = 0; i < MAC_ADDR_SIZE; i++) {
        address[i] = parsed[i];
    }
    return is_valid_address(address);
}

static bool read_nv_address(unsigned char* address)
{
    int pipe_fds[2];
    pid_t child_pid;
    struct pollfd poll_fd;
    int status;
    ssize_t bytes_read;

    if (pipe(pipe_fds) != 0) {
        ALOGE("pipe failed: %s", strerror(errno));
        return false;
    }

    child_pid = fork();
    if (child_pid == 0) {
        unsigned char child_address[MAC_ADDR_SIZE] = {0};
        char* nv_bt_mac = NULL;
        int ret;
        int i;

        close(pipe_fds[0]);
        ret = qmi_nv_read_bd_addr(&nv_bt_mac);
        if (nv_bt_mac != NULL) {
            for (i = 0; i < MAC_ADDR_SIZE; i++) {
                child_address[i] = nv_bt_mac[MAC_ADDR_SIZE - 1 - i];
            }
            if (is_valid_address(child_address)) {
                write(pipe_fds[1], child_address, sizeof(child_address));
            }
        } else {
            ALOGE("qmi_nv_read_bd_addr error %d", ret);
        }
        close(pipe_fds[1]);
        _exit(0);
    }

    close(pipe_fds[1]);
    if (child_pid < 0) {
        ALOGE("fork failed: %s", strerror(errno));
        close(pipe_fds[0]);
        return false;
    }

    poll_fd.fd = pipe_fds[0];
    poll_fd.events = POLLIN;
    status = poll(&poll_fd, 1, NV_READ_TIMEOUT_MS);
    bytes_read = status > 0 ? read(pipe_fds[0], address, MAC_ADDR_SIZE) : -1;
    close(pipe_fds[0]);

    if (status <= 0) {
        ALOGW("Timed out reading Bluetooth address from modem NV");
        kill(child_pid, SIGKILL);
    }
    waitpid(child_pid, NULL, 0);
    return bytes_read == MAC_ADDR_SIZE && is_valid_address(address);
}

static bool generate_local_address(unsigned char* address)
{
    int random_fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);

    if (random_fd < 0 || read(random_fd, address, MAC_ADDR_SIZE) != MAC_ADDR_SIZE) {
        if (random_fd >= 0) close(random_fd);
        ALOGE("Unable to generate fallback Bluetooth address: %s", strerror(errno));
        return false;
    }
    close(random_fd);

    address[0] = (address[0] & 0xfe) | 0x02;
    return true;
}

static bool write_address(const unsigned char* address)
{
    FILE *fp = fopen(BD_ADDR_FILE, "w");

    if (fp == NULL) {
        ALOGE("Unable to open %s: %s", BD_ADDR_FILE, strerror(errno));
        return false;
    }
    fprintf(fp, "%02X:%02X:%02X:%02X:%02X:%02X\n",
            address[0], address[1], address[2], address[3], address[4], address[5]);
    fclose(fp);
    return true;
}

int main(void)
{
    unsigned char bt_addr[MAC_ADDR_SIZE] = {0};

    if (read_existing_address(bt_addr)) {
        ALOGI("Keeping existing Bluetooth address in %s", BD_ADDR_FILE);
        return 0;
    }

    if (!read_nv_address(bt_addr)) {
        if (!generate_local_address(bt_addr)) {
            return 1;
        }
        ALOGW("Using a persistent locally administered Bluetooth address");
    }

    return write_address(bt_addr) ? 0 : 1;
}

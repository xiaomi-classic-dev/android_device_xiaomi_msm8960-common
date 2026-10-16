/*
 * Copyright (C) 2016 The CyanogenMod Project
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

#define _GNU_SOURCE
#define LOG_TAG "bdaddr_xiaomi"
#include <log/log.h>

#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define BD_ADDR_FILE "/data/misc/bluetooth/bdaddr"
#define MAC_ADDR_SIZE 6

static bool valid_address(const uint8_t address[MAC_ADDR_SIZE])
{
    static const uint8_t zero[MAC_ADDR_SIZE] = {0};
    static const uint8_t broadcast[MAC_ADDR_SIZE] = {255, 255, 255, 255, 255, 255};
    return !(address[0] & 1) && memcmp(address, zero, MAC_ADDR_SIZE) &&
            memcmp(address, broadcast, MAC_ADDR_SIZE);
}

static void timed_out(int number)
{
    static const char message[] = "bdaddr_xiaomi: factory address read timed out\n";
    (void)number;
    (void)write(STDERR_FILENO, message, sizeof(message) - 1);
    _exit(124);
}

/* Isolate the old notifier's unbounded wait in an expendable process. */
static int legacy_child(void)
{
    alarm(4);
    void *library = dlopen("libqminvapi.so", RTLD_NOW);
    int (*read_address)(char **) = library ? dlsym(library, "qmi_nv_read_bd_addr") : NULL;
    char *nv = NULL;
    uint8_t address[MAC_ADDR_SIZE];
    if (!read_address || read_address(&nv) || !nv)
        return 1;
    for (int i = 0; i < MAC_ADDR_SIZE; ++i)
        address[i] = (uint8_t)nv[MAC_ADDR_SIZE - 1 - i];
    if (!valid_address(address))
        return 1;
    return write(3, address, sizeof(address)) == (ssize_t)sizeof(address) ? 0 : 1;
}

static bool read_legacy(uint8_t address[MAC_ADDR_SIZE])
{
    int pipes[2];
    if (pipe2(pipes, O_CLOEXEC))
        return false;
    pid_t child = fork();
    if (child == 0) {
        close(pipes[0]);
        if (dup2(pipes[1], 3) < 0 || fcntl(3, F_SETFD, 0) < 0)
            _exit(1);
        if (pipes[1] != 3)
            close(pipes[1]);
        execl("/proc/self/exe", "bdaddr_xiaomi", "--legacy-read", (char *)NULL);
        _exit(1);
    }
    close(pipes[1]);
    if (child < 0) {
        close(pipes[0]);
        return false;
    }
    struct pollfd event = {.fd = pipes[0], .events = POLLIN};
    int ready = poll(&event, 1, 5000);
    ssize_t count = ready > 0 ? read(pipes[0], address, MAC_ADDR_SIZE) : -1;
    close(pipes[0]);
    if (count != MAC_ADDR_SIZE)
        kill(child, SIGKILL);
    int status;
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR)
            return false;
    }
    return count == MAC_ADDR_SIZE && WIFEXITED(status) && WEXITSTATUS(status) == 0 &&
            valid_address(address);
}

static bool saved_address(void)
{
    char text[19] = {0};
    unsigned int values[MAC_ADDR_SIZE];
    uint8_t address[MAC_ADDR_SIZE];
    int fd = open(BD_ADDR_FILE, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0)
        return false;
    ssize_t size = read(fd, text, sizeof(text) - 1);
    close(fd);
    if (size != 17 && !(size == 18 && text[17] == '\n'))
        return false;
    for (int i = 0; i < 17; ++i) {
        if (i % 3 == 2 ? text[i] != ':' : !isxdigit((unsigned char)text[i]))
            return false;
    }
    if (sscanf(text, "%2x:%2x:%2x:%2x:%2x:%2x", &values[0], &values[1],
               &values[2], &values[3], &values[4], &values[5]) != MAC_ADDR_SIZE)
        return false;
    for (int i = 0; i < MAC_ADDR_SIZE; ++i)
        address[i] = values[i];
    return valid_address(address);
}

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--legacy-read"))
        _exit(legacy_child());
    bool probe = argc == 2 && !strcmp(argv[1], "--probe");
    if (argc != 1 && !probe)
        return 2;
    if (!probe && saved_address())
        return 0;
    signal(SIGALRM, timed_out);
    alarm(35);
    uint8_t address[MAC_ADDR_SIZE];
    bool found = false;
    /* Early QMUX clients can remain stuck after the modem becomes ready.
     * Each attempt must exec a fresh client with its own bounded lifetime. */
    for (int attempt = 0; !found && attempt < 6; ++attempt) {
        if (attempt) {
            ALOGI("Retrying Xiaomi NV with a fresh client after modem startup");
            usleep(250000);
        }
        found = read_legacy(address);
    }
    if (!found) {
        ALOGE("Cannot read the factory Bluetooth address after bounded NV attempts");
        return 1;
    }
    ALOGI("Read the factory Bluetooth address using Xiaomi NV");
    alarm(0);
    char text[18];
    snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X", address[0],
             address[1], address[2], address[3], address[4], address[5]);
    if (probe) {
        puts(text);
        return 0;
    }
    const char temporary[] = BD_ADDR_FILE ".tmp";
    int fd = open(temporary, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0) {
        ALOGE("Cannot open the address file: %s", strerror(errno));
        return 1;
    }
    bool failed = fchown(fd, 1002, 1002) || fchmod(fd, 0644);
    if (!failed && write(fd, text, sizeof(text) - 1) != (ssize_t)(sizeof(text) - 1))
        failed = true;
    if (!failed && fsync(fd))
        failed = true;
    if (close(fd))
        failed = true;
    if (!failed && rename(temporary, BD_ADDR_FILE))
        failed = true;
    if (failed) {
        ALOGE("Cannot save the factory address: %s", strerror(errno));
        unlink(temporary);
        return 1;
    }
    ALOGI("Saved the factory Bluetooth address");
    return 0;
}

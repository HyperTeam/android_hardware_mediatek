/*
 * Copyright (C) 2012 The Android Open Source Project
 * Copyright (c) 2020 The LineageOS Project
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

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define LOG_TAG "MTKPowerHAL"
#include <hardware/hardware.h>
#include <hardware/power.h>
#include <linux/input.h>
#include <utils/Log.h>

#include "power.h"

static void power_init(struct power_module* module) {
    if (module) ALOGI("power_init");
}

int sysfs_write(char* path, char* s) {
    char buf[80];
    int len;
    int ret = 0;
    int fd = open(path, O_WRONLY);

    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error opening %s: %s\n", path, buf);
        return -1;
    }

    len = write(fd, s, strlen(s));
    if (len < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing to %s: %s\n", path, buf);

        ret = -1;
    }

    close(fd);

    return ret;
}

static void power_set_interactive(struct power_module* module, int on) {
    if (module) ALOGI("power_set_interactive on:%d", on);
}

int open_ts_input() {
    int fd = -1;
    DIR *dir = opendir("/dev/input");

    if (dir != NULL) {
        struct dirent *ent;

        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_type == DT_CHR) {
                char absolute_path[PATH_MAX] = {0};
                char name[80] = {0};

                strcpy(absolute_path, "/dev/input/");
                strcat(absolute_path, ent->d_name);

                fd = open(absolute_path, O_RDWR);
                if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), &name) > 0) {
                    if (strcmp(name, "atmel_mxt_ts") == 0 || strcmp(name, "fts_ts") == 0 ||
                            strcmp(name, "ft5x46") == 0 || strcmp(name, "synaptics_dsx") == 0 ||
                            strcmp(name, "NVTCapacitiveTouchScreen") == 0)
                        break;
                }

                close(fd);
                fd = -1;
            }
        }

        closedir(dir);
    }

    return fd;
}

static void power_set_feature(struct power_module* module, feature_t feature, int state) {
    if (module) ALOGI("power_set_feature feature:%d, state:%d", feature, state);

    switch (feature) {
        case POWER_FEATURE_DOUBLE_TAP_TO_WAKE: {
            int fd = open_ts_input();
            if (fd == -1) {
                ALOGW("DT2W won't work because no supported touchscreen input devices were found");
                return;
            }
            struct input_event ev;
            ev.type = EV_SYN;
            ev.code = SYN_CONFIG;
            ev.value = state ? INPUT_EVENT_WAKUP_MODE_ON : INPUT_EVENT_WAKUP_MODE_OFF;
            write(fd, &ev, sizeof(ev));
            close(fd);
        } break;
        default:
            break;
    }
}

static void power_hint(struct power_module* module, power_hint_t hint, void* data) {
    int param = 0;

    if (data) param = *((int*)data);

    switch (hint) {
        case POWER_HINT_SUSTAINED_PERFORMANCE:
            if (module) {
                if (param)
                    ALOGI("POWER_HINT_SUSTAINED_PERFORMANCE, on");
                else
                    ALOGI("POWER_HINT_SUSTAINED_PERFORMANCE, off");
            }
            break;
        case POWER_HINT_VR_MODE:
            if (module) {
                if (param)
                    ALOGI("POWER_HINT_VR_MODE, on");
                else
                    ALOGI("POWER_HINT_VR_MODE, off");
            }
            break;
        default:
            break;
    }
}

static int power_open(const hw_module_t* module, const char* name, hw_device_t** device) {
    if (module) ALOGI("%s: enter; name=%s", __FUNCTION__, name);
    int retval = 0; /* 0 is ok; -1 is error */

    if (strcmp(name, POWER_HARDWARE_MODULE_ID) == 0) {
        power_module_t* dev = (power_module_t*)calloc(1, sizeof(power_module_t));

        if (dev) {
            /* Common hw_device_t fields */
            dev->common.tag = HARDWARE_DEVICE_TAG;
            dev->common.module_api_version = POWER_MODULE_API_VERSION_0_3;
            dev->common.hal_api_version = HARDWARE_HAL_API_VERSION;

            dev->init = power_init;
            dev->powerHint = power_hint;
            dev->setInteractive = power_set_interactive;
            dev->setFeature = power_set_feature;
            dev->get_number_of_platform_modes = NULL;
            dev->get_platform_low_power_stats = NULL;
            dev->get_voter_list = NULL;

            *device = (hw_device_t*)dev;
        } else
            retval = -ENOMEM;
    } else {
        retval = -EINVAL;
    }

    ALOGD("%s: exit %d", __FUNCTION__, retval);
    return retval;
}

static struct hw_module_methods_t power_module_methods = {
        .open = power_open,
};

struct power_module HAL_MODULE_INFO_SYM = {
        .common =
                {
                        .tag = HARDWARE_MODULE_TAG,
                        .module_api_version = POWER_MODULE_API_VERSION_0_3,
                        .hal_api_version = HARDWARE_HAL_API_VERSION,
                        .id = POWER_HARDWARE_MODULE_ID,
                        .name = "Mediatek Power HAL",
                        .author = "The Android Open Source Project",
                        .methods = &power_module_methods,
                },

        .init = power_init,
        .setInteractive = power_set_interactive,
        .setFeature = power_set_feature,
        .powerHint = power_hint,
};

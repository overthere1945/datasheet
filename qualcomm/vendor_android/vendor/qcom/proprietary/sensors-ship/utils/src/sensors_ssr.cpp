/*
 * Copyright (c) 2017-2024 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#include <string>
#include <cstring>
#include "sensors_log.h"
#include "sensors_ssr.h"
#ifdef __ANDROID_API__
#include <cutils/properties.h>
#else
#include "properties.h"
#endif
#include <fstream>
#include <unistd.h>
#include <fcntl.h>
#include "sensors_target.h"
using namespace std;


static const char SENSORS_HAL_SSR_SUPPORT[] = "persist.vendor.sensors.debug.hal_trigger_ssr";
static const char SENSORS_HAL_CRASH_SUPPORT[] = "persist.vendor.sensors.debug.hal_trigger_crash";

bool support_ssr_trigger()
{
    char ssr_prop[PROP_VALUE_MAX];
    property_get(SENSORS_HAL_SSR_SUPPORT, ssr_prop, "false");
    sns_logd("ssr_prop: %s",ssr_prop);
    if (!strncmp("true", ssr_prop, 4)) {
        sns_logi("support_ssr_trigger: %s",ssr_prop);
        return true;
    }
    return false;
}

bool support_crash_trigger()
{
    char crash_prop[PROP_VALUE_MAX];
    property_get(SENSORS_HAL_CRASH_SUPPORT, crash_prop, "false");
    sns_logd("crash_prop: %s",crash_prop);
    if (!strncmp("true", crash_prop, 4)) {
        sns_logi("support_crash_trigger: %s",crash_prop);
        return true;
    }
    return false;
}

/* utility function to trigger ssr*/
/*  all deamons/system_app do not have permissions to open
     sys/kernel/boot_slpi/ssr , right now only hal_sensors can do it*/
int trigger_ssr() {
    if(support_ssr_trigger() == true) {
        int _fd = open(sensors_target::get_ssr_path().c_str(), O_WRONLY);

        if (_fd<0) {
            sns_logd("failed to open sys/kernel/boot_*/ssr");
            return -1;
        }
        if (write(_fd, "1", 1) > 0) {
           sns_loge("ssr triggered successfully");
           close(_fd);
           /*allow atleast some time before connecting after ssr*/
           sleep(2);
           return 0;
        } else {
            sns_loge("failed to write sys/kernel/boot_slpi/ssr");
            close(_fd);
            perror("Error: ");
            return -1;
        }
    } else {
        if (support_crash_trigger() == true) {
            int _fd = open("/proc/sysrq-trigger", O_WRONLY | O_CLOEXEC);
            if (_fd < 0) {
                sns_loge("failed to open sysrq-trigger");
                return -1;
            }
            if (write(_fd, "c", 1) > 0) {
                sns_loge("crash triggered successfully");
                close(_fd);
                return 0;
            } else {
                sns_loge("failed to write sysrq-trigger");
                close(_fd);
                perror("Error: ");
                return -1;
            }
        } else {
            sns_logd("trigger_ssr or trigger_crash not supported");
            return -1;
        }
    }
}

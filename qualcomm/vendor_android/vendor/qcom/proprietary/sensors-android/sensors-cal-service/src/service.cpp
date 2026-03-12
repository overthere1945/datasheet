/*
 * Copyright (c) 2023 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */
#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include "SensorsCalibrate.h"
//#define LOG_TAG "vendor.qti.hardware.sensorscalibrate-service"


using aidl::vendor::qti::hardware::sensorscalibrate::SensorsCalibrate;
using ::ndk::ScopedAStatus;

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(0);
    auto sensorcal = ndk::SharedRefBase::make<SensorsCalibrate>();
    const std::string sensorcalName = std::string()+SensorsCalibrate::descriptor + "/default";
    if(sensorcal !=nullptr) {
            if(AServiceManager_registerLazyService(sensorcal->asBinder().get(), sensorcalName.c_str()) != STATUS_OK) {
                ALOGE("failed to register Sensorcalibrate service");
                return -1;
            }
    } else {
        ALOGE("failed to get Sensorcalibrate instance");
        return -1;
    }
    ABinderProcess_joinThreadPool();
    ALOGE("Cannot register SensorsCalibrate HAL service");
    return EXIT_FAILURE;
}

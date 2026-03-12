/*
 * Copyright (c) 2017-2018 2023 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * Not a Contribution.
 * Apache license notifications and license are retained
 * for attribution purposes only.
 *
 * Copyright (C) 2016 The Android Open Source Project
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
#ifndef VENDOR_QTI_HARDWARE_SENSORSCAIBRATE_V1_0_ISENSORSCALIBRATE_H
#define VENDOR_QTI_HARDWARE_SENSORSCAIBRATE_V1_0_ISENSORSCALIBRATE_H

#include <aidl/vendor/qti/hardware/sensorscalibrate/BnSensorsCalibrate.h>
#include <log/log.h>
#include <sensors_calibrate.h>
#include <dlfcn.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "vendor.qti.hardware.sensorscalibrate"

namespace aidl {
namespace vendor {
namespace qti {
namespace hardware {
namespace sensorscalibrate {

using ::aidl::vendor::qti::hardware::sensorscalibrate::ISensorsCalibrate;
using ::ndk::ScopedAStatus;


class SensorsCalibrate : public BnSensorsCalibrate {
  public:
    SensorsCalibrate();
    ScopedAStatus initCheck() const;

    ::ndk::ScopedAStatus SensorsCal(int32_t sensor_type, int8_t test_type, std::string* _aidl_return) override;

private:
    bool mInitCheck;
    sensor_cal_module_t *SensorCalModule;
    void *libsnscal;
    std::string get_datatype(int32_t sensor_type);

};

}  // namespace sensorscalibrate
}  // namespace hardware
}  // namespace qti
}  // namespace vendor
} //namespace aidl

#endif  // VENDOR_QTI_HARDWARE_SENSORSCAIBRATE_V1_0_ISENSORSCALIBRATE_H

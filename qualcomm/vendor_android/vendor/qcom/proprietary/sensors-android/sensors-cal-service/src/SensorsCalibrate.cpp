/*
 * Copyright (c) 2017-2018 2023 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <hardware/sensors.h>
#include "SensorsCalibrate.h"
#include <android-base/logging.h>

using ::ndk::ScopedAStatus;

namespace aidl{
namespace vendor {
namespace qti {
namespace hardware {
namespace sensorscalibrate {


SensorsCalibrate::SensorsCalibrate()
    : mInitCheck(false),
      SensorCalModule(nullptr) {
    bool err = true;;

    libsnscal = dlopen("libsensorcal.so", RTLD_NOW);
    if(!libsnscal) {
        LOG(ERROR) << "Couldn't dlopen"
                   << " sensorcal lib ";
        err = false;
    }
    //Fix for static analysis error - uninitialized variable.
    if (!err) {
      LOG(ERROR) << "Couldn't load "
          << " sensorcal module - dlopen failed";

      mInitCheck = false;
      return;
    }
    SensorCalModule = (struct sensor_cal_module_t *) dlsym(libsnscal, SENSOR_CAL_MODULE_INFO_STR);
    if(!SensorCalModule) {
        err = false;
        LOG(ERROR) << "Couldn't loading"
                   << "symbol"
                   <<SENSOR_CAL_MODULE_INFO_STR;
        dlclose(libsnscal);
        //err = ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    if (!err) {
        LOG(ERROR) << "Couldn't load "
                   << " sensorcal module ";

        mInitCheck = false;
        return;
    }

    mInitCheck = true;
}

std::string SensorsCalibrate::get_datatype(int32_t sensor_type) {

    std::string data_type;
    switch (sensor_type) {
    case SENSOR_TYPE_ACCELEROMETER:
         data_type = "accel";
         break;
    case SENSOR_TYPE_MAGNETIC_FIELD:
         data_type = "mag";
         break;
    case SENSOR_TYPE_GYROSCOPE:
         data_type = "gyro";
         break;
    case SENSOR_TYPE_LIGHT:
         data_type = "ambient_light";
         break;
    case SENSOR_TYPE_AMBIENT_TEMPERATURE:
         data_type ="ambient_temperature";
         break;
    case SENSOR_TYPE_PRESSURE:
         data_type = "pressure";
         break;
    case SENSOR_TYPE_PROXIMITY:
         data_type = "proximity";
         break;
    case SENSOR_TYPE_RELATIVE_HUMIDITY:
         data_type = "humidity";
         break;
    case SENSOR_TYPE_HEART_RATE:
         data_type = "heart_rate";
         break;
    case SENSOR_TYPE_HEART_BEAT:
         data_type = "heart_beat";
         break;
    default:
         break;
    }

    return data_type;

}

ScopedAStatus SensorsCalibrate::SensorsCal(int32_t sensor_type, int8_t test_type,
                        std::string* _aidl_return) {

    std::string str_result;
    std::string data_type_tmp = get_datatype(sensor_type);
    if (!data_type_tmp.empty() && mInitCheck) {
           str_result = SensorCalModule->methods->calibrate(data_type_tmp, test_type);
           *_aidl_return = str_result;
        }
        else {
           *_aidl_return = "sensor type error";
        }
    return ndk::ScopedAStatus::ok();
}


ScopedAStatus SensorsCalibrate::initCheck() const {
    if (mInitCheck)
        return ScopedAStatus::ok();
    return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
}

}  // namespace sensorscalibrate
}  // namespace hardware
}  // namespace qti
}  // namespace vendor
}  //namespace aidl


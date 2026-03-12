/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <mutex>
#include <list>
#include <thread>
#include <cutils/properties.h>
#include "sensor_factory.h"
#include "sensor_calibration.h"
#include "qsh_sensor.h"
#include "sns_cal.pb.h"
#include "sensors_qti.h"

static const char *QSH_DATATYPE_ACCEL = "accel";
static const char *QSH_DATATYPE_ACCELCAL = "accel_cal";
// TODO: Fine tune fifo count value for ultra low target
#ifdef SNS_TARGET_SUPPORTS_ULTRA_LOW_RAM
static const uint32_t ACCEL_RESERVED_FIFO_COUNT = 150;
#elif defined SNS_TARGET_SUPPORTS_NO_HIFI
static const uint32_t ACCEL_RESERVED_FIFO_COUNT = 2000;
#else
static const uint32_t ACCEL_RESERVED_FIFO_COUNT = 3000;
#endif
static const float ONE_G = 9.80665f;
using namespace std;

class accelerometer : public qsh_sensor
{
public:
    accelerometer(suid sensor_uid, sensor_wakeup_type wakeup,
                                        sensor_cal_type calibrated);
    virtual bool is_calibrated() override;

private:
    calibration* _cal = nullptr;
    bool _cal_available;
    suid _cal_suid;
    sensor_cal_type _cal_type;
    virtual void activate() override;
    virtual void deactivate() override;
    /*handle ssr and re initiate the request*/
    virtual void qsh_conn_error_cb(ISession::error e) override;
    virtual void handle_sns_client_event(const sns_client_event_msg_sns_client_event& pb_event);
};

accelerometer::accelerometer(suid sensor_uid,
                                sensor_wakeup_type wakeup,
                                sensor_cal_type cal_type) :
    qsh_sensor(sensor_uid, wakeup),
    _cal_available(false),
    _cal_type(cal_type)
{

  if (cal_type == SENSOR_UNCALIBRATED) {
    set_type(SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED);
    set_string_type(SENSOR_STRING_TYPE_ACCELEROMETER_UNCALIBRATED);
    set_sensor_typename("Accelerometer-Uncalibrated");
  } else {
    set_type(SENSOR_TYPE_ACCELEROMETER);
    set_string_type(SENSOR_STRING_TYPE_ACCELEROMETER);
    set_sensor_typename("Accelerometer");
  }

  if (_cal_available == false) {
    const auto& accelcal_suids =
        sensor_factory::instance().get_suids(QSH_DATATYPE_ACCELCAL);
    if (accelcal_suids.size() == 0) {
      sns_logi("accelcal suid not found");
    } else {
      _cal_suid = accelcal_suids[0];
      _cal_available = true;
    }
  }

  set_fifo_reserved_count(ACCEL_RESERVED_FIFO_COUNT);
  set_resampling(true);
  set_nowk_msgid(SNS_CAL_MSGID_SNS_CAL_EVENT);
  set_nowk_msgid(SNS_STD_MSGID_SNS_STD_RESAMPLER_CONFIG_EVENT);
  set_nowk_msgid(SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_PHYSICAL_CONFIG_EVENT);

  float max_range = get_sensor_info().maxRange;
  float resolution  = get_sensor_info().resolution;
  set_max_range(max_range);
  set_resolution(resolution);
  _qsh_intf_id = qsh_intf_resource_manager::get_instance().register_sensor(get_sensor_info());

  if (true == _cal_available) {
    _cal = new calibration(_cal_suid, QSH_DATATYPE_ACCELCAL, _qsh_intf_id);
    if (nullptr == _cal) {
      sns_loge("failed to create _cal");
      throw runtime_error("fail to create calibration sensor");
    }
  }
}

void accelerometer::qsh_conn_error_cb(ISession::error e)
{
  sns_loge("handle error = %d for accelerometer", e);
  if (e == ISession::RESET) {
    if(true == _is_deactivate_in_progress){
      sns_logi("Accel is being deactivated.");
      return;
    }
    /* re-send accelerometer cal request when connection resets */
    if (_cal_available && nullptr != _cal) {
      try {
        _cal->reactivate();
      }catch (const exception& e) {
        sns_loge("accel cal re-activation failed, %s", e.what());
        return;
      }
    }
    /* do common error handling */
    qsh_sensor::qsh_conn_error_cb(e);
  }
}

void accelerometer::activate()
{
  if (_is_stats_enable) {
    if (_stats == NULL) {
      _stats = new qsh_sensor_stats();
    }
    _stats->start();
  }

  if (!is_active()) {
    acquire_qsh_interface_instance();
    if (true == _cal_available && nullptr != _cal) {
      try {
        _cal->activate();
      } catch (const exception& e) {
        release_qsh_interface_instance();
        sns_loge("accel cal activation failed, %s", e.what());
        throw;
      }
    }

    try{
      start_stream();
    } catch (const exception& e) {
      sns_loge("error in start accelerometer stream, %s", e.what());
      if (true == _cal_available && nullptr != _cal) {
        _cal->deactivate();
      }
      release_qsh_interface_instance();
      throw;
    }
  }
}

void accelerometer::deactivate()
{
  auto release_resources = [this]() {
    release_qsh_interface_instance();
    _is_deactivate_in_progress = false;
    _pending_flush_requests = 0;
    if (_stats != NULL) {
      _stats->stop();
      sns_logi("Sensor (accel) : %s ", _stats->get_stats().c_str());
    }
  };
  if (is_active()) {
    _is_deactivate_in_progress = true;
    if (true == _cal_available && nullptr != _cal) {
      _cal->deactivate();
    }
    try {
      send_sensor_disable_request();
    } catch (const exception& e) {
      sns_loge("sending disable request failed");
      release_resources();
      throw;
    }
    sns_logd("(accelerometer) , _pending_flush_requests=%u " ,(unsigned int) _pending_flush_requests);
    release_resources();
  } else {
    sns_logi("(accelerometer) is not active, no need to deactivate.");
  }
}

void accelerometer::handle_sns_client_event(
        const sns_client_event_msg_sns_client_event& pb_event)
{
    if (SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_EVENT == pb_event.msg_id()) {
        sns_std_sensor_event pb_sensor_event;
        pb_sensor_event.ParseFromString(pb_event.payload());

        if(pb_sensor_event.data_size()) {
            calibration::bias b = {{0.0f,0.0f,0.0f}, 0, SNS_STD_SENSOR_SAMPLE_STATUS_UNRELIABLE};
            Event hal_event = create_sensor_hal_event(pb_event.timestamp());
            /* select calibration bias based on timestamp */
            if(_cal_available) {
              if(nullptr != _cal)
                b = _cal->get_bias(pb_event.timestamp());
            }

            if (_cal_type == SENSOR_CALIBRATED) {
                hal_event.u.vec3.x = pb_sensor_event.data(0) - b.values[0];
                hal_event.u.vec3.y = pb_sensor_event.data(1) - b.values[1];
                hal_event.u.vec3.z = pb_sensor_event.data(2) - b.values[2];
                if(_cal_available){
                    hal_event.u.vec3.status = STD_MIN(qsh_sensor::sensors_hal_sample_status(b.status),
                        qsh_sensor::sensors_hal_sample_status(pb_sensor_event.status()));
                } else {
                    hal_event.u.vec3.status = qsh_sensor::sensors_hal_sample_status(pb_sensor_event.status());
                }
                sns_logd("accel_sample(cal_available:%d): ts=%lld ns; value = [%f, %f, %f], status=%d",
                         (int)_cal_available, (long long) hal_event.timestamp, hal_event.u.vec3.x,
                         hal_event.u.vec3.y,hal_event.u.vec3.z,
                         (int)hal_event.u.vec3.status);
            }
            if (_cal_type == SENSOR_UNCALIBRATED) {
                hal_event.u.uncal.x = pb_sensor_event.data(0);
                hal_event.u.uncal.y = pb_sensor_event.data(1);
                hal_event.u.uncal.z = pb_sensor_event.data(2);
                hal_event.u.uncal.x_bias = b.values[0];
                hal_event.u.uncal.y_bias = b.values[1];
                hal_event.u.uncal.z_bias = b.values[2];
                sns_logd("accel_sample_uncal: ts=%lld ns; value = [%f, %f, %f],"
                         " bias = [%f, %f, %f]", (long long) hal_event.timestamp,
                          hal_event.u.uncal.x,
                          hal_event.u.uncal.y,
                          hal_event.u.uncal.z,
                          hal_event.u.uncal.x_bias,
                          hal_event.u.uncal.y_bias,
                          hal_event.u.uncal.z_bias);
            }
            if(_stats != NULL)
              _stats->update_sample(hal_event.timestamp, pb_event.timestamp());

            if(true == can_submit_sample(hal_event))
              events.push_back(hal_event);

        } else {
            sns_loge("empty data returned for accel");
        }
    }
}

bool accelerometer::is_calibrated()
{
  return (_cal_type == SENSOR_CALIBRATED) ? true : false;
}

/* create sensor variants supported by this class and register the function
   to sensor_factory */
static vector<unique_ptr<sensor>> get_available_accel_calibrated()
{
    vector<unique_ptr<sensor>> sensors;
    const vector<suid>& accel_suids =
          sensor_factory::instance().get_suids(QSH_DATATYPE_ACCEL);

    for (const auto& suid : accel_suids) {
        try {
            sensors.push_back(make_unique<accelerometer>(suid, SENSOR_WAKEUP,
                                                     SENSOR_CALIBRATED));
        } catch (const exception& e) {
            sns_loge("failed for wakeup, %s", e.what());
        }
        try {
            sensors.push_back(make_unique<accelerometer>(suid, SENSOR_NO_WAKEUP,
                                                     SENSOR_CALIBRATED));
        } catch (const exception& e) {
            sns_loge("failed for nowakeup, %s", e.what());
        }
    }
    return sensors;
}

static vector<unique_ptr<sensor>> get_available_accel_uncalibrated()
{
    vector<unique_ptr<sensor>> sensors;
    const vector<suid>& accel_suids =
         sensor_factory::instance().get_suids(QSH_DATATYPE_ACCEL);

    for (const auto& suid : accel_suids) {
        try {
            sensors.push_back(make_unique<accelerometer>(suid, SENSOR_WAKEUP,
                                                     SENSOR_UNCALIBRATED));
        } catch (const exception& e) {
            sns_loge("failed for wakeup,  %s", e.what());
        }
        try {
            sensors.push_back(make_unique<accelerometer>(suid, SENSOR_NO_WAKEUP,
                                                     SENSOR_UNCALIBRATED));
        } catch (const exception& e) {
            sns_loge("failed for nowakeup, %s", e.what());
        }
    }
    return sensors;
}

static bool accelerometer_module_init()
{
    /* register supported sensor types with factory */
    sensor_factory::register_sensor(SENSOR_TYPE_ACCELEROMETER,
                                    get_available_accel_calibrated);
    sensor_factory::register_sensor(SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED,
                                    get_available_accel_uncalibrated);
    sensor_factory::request_datatype(QSH_DATATYPE_ACCEL);

    char debug_prop[PROPERTY_VALUE_MAX];
    int enable_accel_cal = 0;
    int len;
    len = property_get("persist.vendor.sensors.accel_cal", debug_prop, "0");
    if (len > 0) {
        enable_accel_cal = atoi(debug_prop);
    }
    if (enable_accel_cal)
    {
      sensor_factory::request_datatype(QSH_DATATYPE_ACCELCAL);
    }
    return true;
}

SENSOR_MODULE_INIT(accelerometer_module_init);

/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <mutex>
#include <list>
#include <thread>

#include "sensor_factory.h"
#include "qsh_sensor.h"
#include "sensor_calibration.h"
#include "sns_cal.pb.h"
#include "additionalinfo_sensor.h"
#include "sensors_qti.h"

using namespace std;

using namespace std::placeholders;
static const char *QSH_DATATYPE_GYRO = "gyro";
static const char *QSH_DATATYPE_GYROCAL = "gyro_cal";
static const char *QSH_DATATYPE_TEMP    = "sensor_temperature";

/**
 * @brief class implementing both calibrated and uncalibrated
 *        gyroscope sensors
 *
 */
class gyroscope : public qsh_sensor
{
public:
    gyroscope(suid gyro_suid, sensor_wakeup_type wakeup,
              sensor_cal_type calibrated);
    virtual bool is_calibrated() override;
private:

    bool _cal_available = false;
    calibration* _cal = nullptr;
    suid _temp_suid;
    suid _cal_suid;
    sensor_cal_type _cal_type;
    bool _temp_available = false;
    additionalinfo_sensor* _temp_ptr = nullptr;
    sensor_wakeup_type _temp_wakeup_type;

    virtual void activate() override;
    virtual void deactivate() override;
    virtual void register_event_callback(sensor_event_cb_func cb) override;
    /*handle ssr and re initiate the request*/
    virtual void qsh_conn_error_cb(ISession::error e) override;
    void handle_sns_client_event(const sns_client_event_msg_sns_client_event& pb_event);
};

gyroscope::gyroscope(suid gyro_suid, sensor_wakeup_type wakeup,
                     sensor_cal_type calibrated):
    qsh_sensor(gyro_suid, wakeup),
    _cal_type(calibrated),
    _temp_wakeup_type(wakeup)
{
  if (_cal_type == SENSOR_CALIBRATED) {
    set_type(SENSOR_TYPE_GYROSCOPE);
    set_string_type(SENSOR_STRING_TYPE_GYROSCOPE);
    set_sensor_typename("Gyroscope");
  } else {
    set_type(SENSOR_TYPE_GYROSCOPE_UNCALIBRATED);
    set_string_type(SENSOR_STRING_TYPE_GYROSCOPE_UNCALIBRATED);
    set_sensor_typename("Gyroscope-Uncalibrated");
  }

  if (_cal_available == false) {
    const auto& gyrocal_suids =
        sensor_factory::instance().get_suids(QSH_DATATYPE_GYROCAL);
    if (gyrocal_suids.size() == 0) {
      sns_logi("gyrocal suid not found");
    } else {
      _cal_suid = gyrocal_suids[0];
      _cal_available = true;
    }
  }

  if(!sensor_factory::instance().get_pairedsuid(
      QSH_DATATYPE_TEMP,
      gyro_suid,
      _temp_suid)) {
    _temp_available = true;
    set_additional_info_flag(true);
    sns_logd("gyro temperature is available..");
  }
  set_resampling(true);
  set_nowk_msgid(SNS_CAL_MSGID_SNS_CAL_EVENT);
  set_nowk_msgid(SNS_STD_MSGID_SNS_STD_RESAMPLER_CONFIG_EVENT);
  set_nowk_msgid(SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_PHYSICAL_CONFIG_EVENT);

  float max_range = get_sensor_info().maxRange;
  float resolution = get_sensor_info().resolution;
  set_max_range(max_range);
  set_resolution(resolution);
  _qsh_intf_id = qsh_intf_resource_manager::get_instance().register_sensor(get_sensor_info());

  if (true == _cal_available) {
    _cal = new calibration(_cal_suid, QSH_DATATYPE_GYROCAL, _qsh_intf_id);
    if (nullptr == _cal) {
      sns_loge("failed to create _cal");
      throw runtime_error("fail to create calibration sensor");
    }
  }
  if (true == _temp_available) {
    _temp_ptr = new additionalinfo_sensor(_temp_suid, _temp_wakeup_type, get_sensor_info().handle, _qsh_intf_id);
  }
}

void gyroscope::register_event_callback(sensor_event_cb_func cb)
{
  _event_cb = cb;
  if (_temp_available) {
    sns_logi("register event callback to temp sensor");
    _temp_ptr->register_event_callback(_event_cb);
  }
}

void gyroscope::qsh_conn_error_cb(ISession::error e)
{
  sns_loge("handle %d error for gyroscope", e);
  if (e == ISession::RESET) {
    if (true == _is_deactivate_in_progress) {
      sns_logi("Gyro is being deactivated.");
      return;
    }
    /* re-send gyroscope cal request when connection resets */
    if (true == _cal_available && nullptr != _cal) {
      try {
        _cal->reactivate();
      } catch (const exception& e) {
        sns_loge("gyro cal re-activation failed, %s", e.what());
        return;
      }
    }
    /* do common error handling */
    qsh_sensor::qsh_conn_error_cb(e);
  }
}

void gyroscope::activate()
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
        sns_loge("gyro cal activation failed, %s", e.what());
        throw;
      }
    }

    if (true == _temp_available && nullptr != _temp_ptr) {
      sensor_params config_params = get_params();
      /* recommended rate is 1/1000 of master */
      config_params.sample_period_ns = get_params().sample_period_ns * 1000;
      _temp_ptr->set_config(config_params);
      try {
        _temp_ptr->activate();
      } catch (const exception& e) {
        sns_loge("error in activating temperature sensor, %s", e.what());
        if (true == _cal_available && nullptr != _cal) {
          _cal->deactivate();
        }
        release_qsh_interface_instance();
        throw;
      }
    }

    try {
      start_stream();
    } catch (const exception& e) {
      sns_loge("error in start gyro stream. hence closing gyro temp and deactivate cal sensor, %s",e.what());
      if (_temp_available) {
        try {
          _temp_ptr->deactivate();
        } catch (const exception& e) {
          sns_loge("fail to deactivate temp sensor while handling the exception of start_stream");
        }
      }
      if (true == _cal_available && nullptr != _cal) {
        _cal->deactivate();
      }
      release_qsh_interface_instance();
      throw;
    }
  }
}

void gyroscope::deactivate()
{
  auto release_resources = [this](){
    release_qsh_interface_instance();
    _is_deactivate_in_progress = false;
    _pending_flush_requests = 0;
    if (_stats != NULL) {
      _stats->stop();
      sns_logi("Sensor (gyro) : %s ", _stats->get_stats().c_str());
    }
  };

  if (is_active()) {
    _is_deactivate_in_progress = true;
    if (_temp_available && _temp_ptr) {
      try {
        _temp_ptr->deactivate();
      } catch (const exception& e) {
        sns_loge("fail to deactivate temp sensor. proceed to deactivate gyro");
      }
    }

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
    sns_logd("(gyroscope) , _pending_flush_requests=%u " ,(unsigned int) _pending_flush_requests);
    release_resources();
  } else {
    sns_logi("(gyroscope) is not active, no need to deactivate.");
  }
}

void gyroscope::handle_sns_client_event(const sns_client_event_msg_sns_client_event& pb_event)
{
    if (SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_EVENT == pb_event.msg_id()) {
        sns_std_sensor_event pb_sensor_event;
        pb_sensor_event.ParseFromString(pb_event.payload());

        if(pb_sensor_event.data_size()) {
            calibration::bias b = {{0.0f,0.0f,0.0f}, 0, SNS_STD_SENSOR_SAMPLE_STATUS_UNRELIABLE};
            Event hal_event = create_sensor_hal_event(pb_event.timestamp());
            /* select calibration bias based on timestamp */

            if(_cal_available){
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
                }
                else {
                  hal_event.u.vec3.status = qsh_sensor::sensors_hal_sample_status(pb_sensor_event.status());
                }
                sns_logd("gyro_sample (cal_available:%d): ts=%lld ns; value = [%f, %f, %f], status = %d",
                         (int)_cal_available,(long long) hal_event.timestamp,
                         hal_event.u.vec3.x, hal_event.u.vec3.y, hal_event.u.vec3.z,
                         (int)hal_event.u.vec3.status);
            } else {
                hal_event.u.uncal.x = pb_sensor_event.data(0);
                hal_event.u.uncal.y = pb_sensor_event.data(1);
                hal_event.u.uncal.z = pb_sensor_event.data(2);
                hal_event.u.uncal.x_bias = b.values[0];
                hal_event.u.uncal.y_bias = b.values[1];
                hal_event.u.uncal.z_bias = b.values[2];
                sns_logd("gyro_sample_uncal: ts=%lld ns; value = [%f, %f, %f],"
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
            sns_loge("empty data returned for gyro");
        }

    }
}

bool gyroscope::is_calibrated()
{
  return (_cal_type == SENSOR_CALIBRATED) ? true : false;
}

/* create sensor variants supported by this class and register the function
   to sensor_factory */
static vector<unique_ptr<sensor>> get_available_gyroscopes_calibrated()
{
    vector<unique_ptr<sensor>> sensors;
    const vector<suid>& gyro_suids =
            sensor_factory::instance().get_suids(QSH_DATATYPE_GYRO);
    for (const auto& sensor_uid : gyro_suids) {
        try {
            sensors.push_back(make_unique<gyroscope>(sensor_uid,
                                    SENSOR_WAKEUP, SENSOR_CALIBRATED));
        } catch (const exception& e) {
            sns_loge("failed for wakeup, %s", e.what());
        }
        try {
            sensors.push_back(make_unique<gyroscope>(sensor_uid,
                                    SENSOR_NO_WAKEUP, SENSOR_CALIBRATED));
        } catch (const exception& e) {
            sns_loge("failed for nowakeup, %s", e.what());
        }
    }
    return sensors;
}

static vector<unique_ptr<sensor>> get_available_gyroscopes_uncalibrated()
{
    vector<unique_ptr<sensor>> sensors;
    const vector<suid>& gyro_suids =
            sensor_factory::instance().get_suids(QSH_DATATYPE_GYRO);

    for(const auto& sensor_uid : gyro_suids) {
        try {
            sensors.push_back(make_unique<gyroscope>(sensor_uid,
                                    SENSOR_WAKEUP, SENSOR_UNCALIBRATED));
        } catch (const exception& e) {
            sns_loge("failed for wakeup,  %s", e.what());
        }
        try {
            sensors.push_back(make_unique<gyroscope>(sensor_uid,
                                    SENSOR_NO_WAKEUP, SENSOR_UNCALIBRATED));
        } catch (const exception& e) {
            sns_loge("failed for nowakeup, %s", e.what());
        }
    }
    return sensors;
}

static bool gyroscope_module_init()
{
    /* register supported sensor types with factory */
    sensor_factory::register_sensor(SENSOR_TYPE_GYROSCOPE,
                                    get_available_gyroscopes_calibrated);
    sensor_factory::register_sensor(SENSOR_TYPE_GYROSCOPE_UNCALIBRATED,
                                    get_available_gyroscopes_uncalibrated);
    sensor_factory::request_datatype(QSH_DATATYPE_GYRO);
    sensor_factory::request_datatype(QSH_DATATYPE_GYROCAL);
    sensor_factory::request_datatype(QSH_DATATYPE_TEMP);
    return true;
}

SENSOR_MODULE_INIT(gyroscope_module_init);

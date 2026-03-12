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
#include "qsh_sensor.h"
#include "sns_cal.pb.h"
#include "sensor_calibration.h"

using namespace std;

static const char *QSH_DATATYPE_MAG = "mag";
static const char *QSH_DATATYPE_MAGCAL = "mag_cal";
static const uint32_t MAG_RESERVED_FIFO_COUNT = 600;
static const char SENSORS_HAL_PROP_ENABLE_MAG_FILTER[] = "persist.vendor.sensors.enable.mag_filter";

#define MAG_FILTER_LENGTH (8)
#define MAG_AXES (3)

/**
 * @brief class implementing both calibrated and uncalibrated
 *        magnetometer sensors
 *
 */
class magnetometer : public qsh_sensor
{
public:

    magnetometer(suid mag_suid, sensor_wakeup_type wakeup,
                 sensor_cal_type calibrated);
    virtual bool is_calibrated() override;
private:
    sensor_cal_type _cal_type;
    calibration* _cal = nullptr;
    bool _cal_available;
    suid _cal_suid;
    void handle_sns_client_event(
        const sns_client_event_msg_sns_client_event& pb_event);
    virtual void activate() override;
    virtual void deactivate() override;
    /*handle ssr and re initiate the request*/
    virtual void qsh_conn_error_cb(ISession::error e) override;
    /* variable for averaging filter */
    bool _filter_enabled;
    uint8_t _filter_buff_index;
    double _filter_buff[MAG_AXES][MAG_FILTER_LENGTH];
    double _filter_sum[MAG_AXES];
    bool _filter_buff_full;
};

magnetometer::magnetometer(suid sensor_uid, sensor_wakeup_type wakeup,
                     sensor_cal_type calibrated):
    qsh_sensor(sensor_uid, wakeup),
    _cal_type(calibrated),
    _cal_available(false),
    _filter_enabled(false)
{
    if (_cal_type == SENSOR_CALIBRATED) {
        set_type(SENSOR_TYPE_MAGNETIC_FIELD);
        set_string_type(SENSOR_STRING_TYPE_MAGNETIC_FIELD);
        set_sensor_typename("Magnetometer");
    } else {
        set_type(SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED);
        set_string_type(SENSOR_STRING_TYPE_MAGNETIC_FIELD_UNCALIBRATED);
        set_sensor_typename("Magnetometer-Uncalibrated");
    }

    if (_cal_available == false) {
      const auto& magcal_suids =
          sensor_factory::instance().get_suids(QSH_DATATYPE_MAGCAL);
      if (magcal_suids.size() == 0) {
        sns_logi("magcal suid not found");
      } else {
        _cal_suid = magcal_suids[0];
        _cal_available = true;
      }
    }

    char prop_val[PROPERTY_VALUE_MAX];
    property_get(SENSORS_HAL_PROP_ENABLE_MAG_FILTER, prop_val, "false");
    if (!strncmp("true", prop_val, 4)) {
        _filter_enabled = true;
    }

    set_fifo_reserved_count(MAG_RESERVED_FIFO_COUNT);
    set_resampling(true);
    set_nowk_msgid(SNS_CAL_MSGID_SNS_CAL_EVENT);
    set_nowk_msgid(SNS_STD_MSGID_SNS_STD_RESAMPLER_CONFIG_EVENT);
    set_nowk_msgid(SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_PHYSICAL_CONFIG_EVENT);
    _qsh_intf_id = qsh_intf_resource_manager::get_instance().register_sensor(get_sensor_info());

    if (true == _cal_available) {
      _cal = new calibration(_cal_suid, QSH_DATATYPE_MAGCAL, _qsh_intf_id);
      if(nullptr == _cal) {
        sns_loge("failed to create _cal");
        throw runtime_error("fail to create calibration sensor");
      }
    }
}

void magnetometer::qsh_conn_error_cb(ISession::error e)
{
  sns_loge("handle error = %d for magnetometer", e);
  if (e == ISession::RESET) {
    if(true == _is_deactivate_in_progress){
      sns_logi("Mag is being deactivated.");
      return;
    }
    /* re-send magnetometer cal request when connection resets */
    if (_cal_available && nullptr != _cal){
      try {
        _cal->reactivate();
      } catch(const exception& e) {
        sns_loge("mag cal re-activation failed, %s", e.what());
        return;
      }
    }
    /* do common error handling */
    qsh_sensor::qsh_conn_error_cb(e);
  }
}

void magnetometer::activate()
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
      } catch (const exception e) {
        release_qsh_interface_instance();
        sns_loge("mag cal activation failed, %s", e.what());
        throw;
      }
    }

    try {
      start_stream();
    } catch (const exception& e) {
      sns_loge("error in start magnetometer stream, %s", e.what());
      if (true == _cal_available && nullptr != _cal) {
        _cal->deactivate();
      }
      release_qsh_interface_instance();
      throw;
    }
    sns_logi("mag_filter is %s", _filter_enabled ? "enabled":"disabled");
    /* initialize averaging filter variables */
    _filter_buff_full = false;
    _filter_buff_index = 0;
    for(int i = 0; i < MAG_AXES; i++) {
      for(int j = 0; j < MAG_FILTER_LENGTH; j++) {
        _filter_buff[i][j] = 0.0f;
      }
      _filter_sum[i] = 0.0f;
    }
  }
}

void magnetometer::deactivate()
{
  auto release_resources = [this]() {
    release_qsh_interface_instance();
    _is_deactivate_in_progress = false;
    _pending_flush_requests = 0;
    if(_stats != NULL){
      _stats->stop();
      sns_logi("Sensor (Magnetometer) : %s ", _stats->get_stats().c_str());
    }
  };

  if (is_active()) {
    _is_deactivate_in_progress = true;
    if (_cal_available && nullptr != _cal) {
        _cal->deactivate();
    }

    try{
      send_sensor_disable_request();
    } catch (const exception& e) {
      sns_loge("sending disable request failed");
      release_resources();
      throw;
    }
    sns_logi("(magnetometer) , _pending_flush_requests=%u " ,(unsigned int) _pending_flush_requests);
    release_resources();
  } else{
    sns_logd("(magnetometer) is not active, no need to deactivate.");
  }
}

void magnetometer::handle_sns_client_event(
        const sns_client_event_msg_sns_client_event& pb_event)
{
    if (SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_EVENT == pb_event.msg_id()) {
            sns_std_sensor_event pb_sensor_event;
            double mag_data[MAG_AXES] = { 0.0f, 0.0f, 0.0f };
            pb_sensor_event.ParseFromString(pb_event.payload());

            if(pb_sensor_event.data_size()) {
                calibration::bias b = {{0.0f,0.0f,0.0f}, 0, SNS_STD_SENSOR_SAMPLE_STATUS_UNRELIABLE};
                Event hal_event = create_sensor_hal_event(pb_event.timestamp());
                /* select calibration bias based on timestamp */
                if(_cal_available){
                  if(nullptr != _cal)
                    b = _cal->get_bias(pb_event.timestamp());
                }

                if (_filter_enabled) {
                    /* applying averaging filter in HAL */
                    if (_filter_buff_index < MAG_FILTER_LENGTH) {
                        _filter_sum[0] += pb_sensor_event.data(0) - _filter_buff[0][_filter_buff_index];
                        _filter_sum[1] += pb_sensor_event.data(1) - _filter_buff[1][_filter_buff_index];
                        _filter_sum[2] += pb_sensor_event.data(2) - _filter_buff[2][_filter_buff_index];

                        _filter_buff[0][_filter_buff_index] = pb_sensor_event.data(0);
                        _filter_buff[1][_filter_buff_index] = pb_sensor_event.data(1);
                        _filter_buff[2][_filter_buff_index] = pb_sensor_event.data(2);
                    }

                    if (_filter_buff_full) {
                        mag_data[0] = (double)(_filter_sum[0]/MAG_FILTER_LENGTH);
                        mag_data[1] = (double)(_filter_sum[1]/MAG_FILTER_LENGTH);
                        mag_data[2] = (double)(_filter_sum[2]/MAG_FILTER_LENGTH);
                    } else {
                        mag_data[0] = (double)(_filter_sum[0]/(_filter_buff_index+1));
                        mag_data[1] = (double)(_filter_sum[1]/(_filter_buff_index+1));
                        mag_data[2] = (double)(_filter_sum[2]/(_filter_buff_index+1));
                    }
                    sns_logv("mag_filtering: value [%f, %f, %f], filtered value [%f, %f, %f], index = %d, full = %d",
                                 pb_sensor_event.data(0), pb_sensor_event.data(1), pb_sensor_event.data(2),
                                 mag_data[0], mag_data[1], mag_data[2],
                                 _filter_buff_index, _filter_buff_full);

                    _filter_buff_index++;
                    if (_filter_buff_index == MAG_FILTER_LENGTH) {
                        _filter_buff_index = 0;
                        _filter_buff_full = true;
                    }
                } else {
                    mag_data[0] = pb_sensor_event.data(0);
                    mag_data[1] = pb_sensor_event.data(1);
                    mag_data[2] = pb_sensor_event.data(2);
                }

                if (_cal_type == SENSOR_CALIBRATED) {
                    hal_event.u.vec3.x = mag_data[0] - b.values[0];
                    hal_event.u.vec3.y = mag_data[1] - b.values[1];
                    hal_event.u.vec3.z = mag_data[2] - b.values[2];
                    if(_cal_available){
                      hal_event.u.vec3.status = STD_MIN(qsh_sensor::sensors_hal_sample_status(b.status),
                          qsh_sensor::sensors_hal_sample_status(pb_sensor_event.status()));
                    }
                    else {
                      hal_event.u.vec3.status = qsh_sensor::sensors_hal_sample_status(pb_sensor_event.status());
                    }
                    sns_logd("mag_sample (magcal_available:%d): ts=%lld ns; value = [%f, %f, %f], status=%d",
                             (int)_cal_available, (long long) hal_event.timestamp, hal_event.u.vec3.x,
                             hal_event.u.vec3.y, hal_event.u.vec3.z,
                             (int)hal_event.u.vec3.status);
                } else {
                    hal_event.u.uncal.x = mag_data[0];
                    hal_event.u.uncal.y = mag_data[1];
                    hal_event.u.uncal.z = mag_data[2];
                    hal_event.u.uncal.x_bias = b.values[0];
                    hal_event.u.uncal.y_bias = b.values[1];
                    hal_event.u.uncal.z_bias = b.values[2];

                    sns_logd("mag_sample_uncal: ts=%lld ns; value = [%f, %f, %f],"
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
                sns_loge("empty data returned for mag");
            }
        }
}

bool magnetometer::is_calibrated()
{
  return (_cal_type == SENSOR_CALIBRATED) ? true : false;
}

/* create all variants for calibrated mag sensor */
static vector<unique_ptr<sensor>> get_available_magnetometers_calibrated()
{
    vector<unique_ptr<sensor>> sensors;
    const vector<suid>& mag_suids =
            sensor_factory::instance().get_suids(QSH_DATATYPE_MAG);

    for (const auto& sensor_uid : mag_suids) {
        try {
            sensors.push_back(make_unique<magnetometer>(sensor_uid, SENSOR_WAKEUP,
                                                        SENSOR_CALIBRATED));
        } catch (const exception& e) {
            sns_loge("failed for wakeup, %s", e.what());
        }
        try {
            sensors.push_back(make_unique<magnetometer>(sensor_uid, SENSOR_NO_WAKEUP,
                                                        SENSOR_CALIBRATED));
        } catch (const exception& e) {
            sns_loge("failed for nowakeup, %s", e.what());
        }
    }
    return sensors;
}

/* create all variants for uncalibrated mag sensor */
static vector<unique_ptr<sensor>> get_available_magnetometers_uncalibrated()
{
    vector<unique_ptr<sensor>> sensors;
    const vector<suid>& mag_suids =
            sensor_factory::instance().get_suids(QSH_DATATYPE_MAG);

    for (const auto& sensor_uid : mag_suids) {
        try{
            sensors.push_back(make_unique<magnetometer>(sensor_uid, SENSOR_WAKEUP,
                                                        SENSOR_UNCALIBRATED));
        } catch (const exception& e) {
            sns_loge("failed for wakeup,  %s", e.what());
        }
        try {
            sensors.push_back(make_unique<magnetometer>(sensor_uid, SENSOR_NO_WAKEUP,
                                                        SENSOR_UNCALIBRATED));
        } catch (const exception& e) {
            sns_loge("failed for nowakeup, %s", e.what());
        }
    }
    return sensors;
}

static bool magnetometer_module_init()
{
    /* register supported sensor types with factory */
    sensor_factory::register_sensor(SENSOR_TYPE_MAGNETIC_FIELD,
                                    get_available_magnetometers_calibrated);
    sensor_factory::register_sensor(SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED,
                                    get_available_magnetometers_uncalibrated);
    sensor_factory::request_datatype(QSH_DATATYPE_MAG);
    sensor_factory::request_datatype(QSH_DATATYPE_MAGCAL);
    return true;
}

SENSOR_MODULE_INIT(magnetometer_module_init);

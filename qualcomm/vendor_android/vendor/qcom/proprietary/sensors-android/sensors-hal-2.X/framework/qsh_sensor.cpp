/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <thread>
#include <chrono>
#include <mutex>
#include <string.h>
#include "qsh_sensor.h"
#include "sensors_log.h"
#include "sensors_ssr.h"
#include "sensors_qti.h"
using namespace std;
using namespace std::chrono;

#define SENSOR_HANDLE_RANGE (10)
#define QTI_SENSOR_TYPE_HANDLE_BASE (1000)
#define SSR_ATTRIBUTE_RETRIEVAL_WAIT_TIME_MS (2000)

static char DIAG_LOG_MODULE_NAME[] = "sensors-hal";


void qsh_sensor_stats::start(){
  _activate_ts = android::elapsedRealtimeNano();
  _ts_diff_acc = 0;
  _sample_cnt = 0;
  _max_ts_rxved = 0;
  _min_ts_rxved = std::numeric_limits<uint64_t>::max();

  _ts_diff_acc_hal = 0;
  _max_ts_rxved_hal = 0;
  _min_ts_rxved_hal = std::numeric_limits<uint64_t>::max();

  _ts_diff_acc_qmi = 0;
  _max_ts_rxved_qmi = 0;
  _min_ts_rxved_qmi = std::numeric_limits<uint64_t>::max();

  _previous_qsh_ts = 0;
  _current_qsh_ts = 0;
  _qsh_ts_diff_bw_samples = 0;
  _acc_qsh_ts = 0;
  _min_qsh_ts_diff = std::numeric_limits<int64_t>::max();
  _max_qsh_ts_diff = 0;
}

void qsh_sensor_stats::stop(){
  _deactive_ts = android::elapsedRealtimeNano();
  _served_duration = _deactive_ts - _activate_ts;
}

void qsh_sensor_stats::add_sample(uint64_t received_ts){
  _sample_added = true;
  _sample_received_ts = received_ts;
}

void qsh_sensor_stats::update_sample(uint64_t meas_ts, uint64_t hal_ts){
  if(true == _sample_added){
    //Calculate sample interval
    if(_sample_cnt == 0) {
      _previous_qsh_ts = (unsigned long long)hal_ts;
    } else {
      _current_qsh_ts = (unsigned long long)hal_ts;
      _qsh_ts_diff_bw_samples = _current_qsh_ts - _previous_qsh_ts;
      _acc_qsh_ts += _qsh_ts_diff_bw_samples;

      if(_qsh_ts_diff_bw_samples < _min_qsh_ts_diff)
        _min_qsh_ts_diff = _qsh_ts_diff_bw_samples;

      if(_qsh_ts_diff_bw_samples > _max_qsh_ts_diff) {
        _max_qsh_ts_diff = _qsh_ts_diff_bw_samples;
        sns_loge("Updating max now to _max_qsh_ts_diff %llu, at _sample_cnt %llu , _current_qsh_ts %llu , previous ts %llu " ,
            (unsigned long long)_max_qsh_ts_diff ,
            (unsigned long long)_sample_cnt ,
            (unsigned long long)_current_qsh_ts ,
            (unsigned long long)_previous_qsh_ts);
      }
      _previous_qsh_ts = _current_qsh_ts;
    }

    //Calculate latency
    uint64_t ts_diff = android::elapsedRealtimeNano() - (unsigned long long)meas_ts;
    uint64_t ts_diff_hal = android::elapsedRealtimeNano() - (unsigned long long)_sample_received_ts;
    uint64_t ts_diff_qmi_propagation = (unsigned long long)_sample_received_ts - (unsigned long long)meas_ts;
    sns_logv("ts_diff %llu , ts_diff_hal %llu , ts_diff_qmi_propagation %llu", (unsigned long long)ts_diff , (unsigned long long)ts_diff_hal , (unsigned long long)ts_diff_qmi_propagation);
    _ts_diff_acc += ts_diff;
    if (_max_ts_rxved < ts_diff) _max_ts_rxved = ts_diff;
    if (_min_ts_rxved > ts_diff) _min_ts_rxved = ts_diff;

    _ts_diff_acc_hal += ts_diff_hal;
    if (_max_ts_rxved_hal < ts_diff_hal) _max_ts_rxved_hal = ts_diff_hal;
    if (_min_ts_rxved_hal > ts_diff_hal) _min_ts_rxved_hal = ts_diff_hal;

    _ts_diff_acc_qmi += ts_diff_qmi_propagation;
    if (_max_ts_rxved_qmi < ts_diff_qmi_propagation) _max_ts_rxved_qmi = ts_diff_qmi_propagation;
    if (_min_ts_rxved_qmi > ts_diff_qmi_propagation) _min_ts_rxved_qmi = ts_diff_qmi_propagation;

    _sample_cnt++;
    _sample_added = false;
  } else{
    sns_loge("Add sample not called for this sample");
  }
}

string qsh_sensor_stats::get_stats(){
  string result = "";
  result += "Activated at "+ to_string(_activate_ts) + "\n";
  result += "Deactivated at "+ to_string(_deactive_ts) + "\n";

  if(_sample_cnt == 0) {
    result +="Served 0 samples for a duration of "+to_string(_sample_cnt)+"ms\n";
  } else {
    result +=  "served "+ to_string((unsigned long long)_sample_cnt) +
    "samples for a duration of " + to_string((double)_served_duration/NSEC_PER_MSEC) +
    " ms, with SR " + to_string((double)_served_duration/(_sample_cnt)) + "\n";

    result += "Latency till HAL CB Triggered: Min: " + to_string((double)_min_ts_rxved_qmi/NSEC_PER_MSEC) +
    " ms , Max: " + to_string((double)_max_ts_rxved_qmi/NSEC_PER_MSEC) +
    " ms , Avg: "+ to_string(((double)_ts_diff_acc_qmi/(_sample_cnt*NSEC_PER_MSEC))) + " ms\n";

    result += "Latency only at HAL: Min: " + to_string((double)_min_ts_rxved_hal/NSEC_PER_MSEC) +
    " ms , Max: " + to_string((double)_max_ts_rxved_hal/NSEC_PER_MSEC) +
    " ms , Avg: " + to_string(((double)_ts_diff_acc_hal/(_sample_cnt*NSEC_PER_MSEC))) + " ms\n";

    result += "End to End Total Latency till HAL: Min: " + to_string((double)_min_ts_rxved/NSEC_PER_MSEC) +
    " ms , Max: " + to_string((double)_max_ts_rxved/NSEC_PER_MSEC) +
    " ms , Avg: " + to_string(((double)_ts_diff_acc/(_sample_cnt*NSEC_PER_MSEC))) + " ms\n";

    result += "from qsh_ticks based, avg sample interval " + to_string((double)_acc_qsh_ts/(_sample_cnt*19200)) +
    " msec, Min is " + to_string((double)_min_qsh_ts_diff/19200) +
    " , Max is " + to_string((double)_max_qsh_ts_diff/19200) +
    " , total sample count " + to_string((unsigned long long)_sample_cnt) + "\n";
  }
  return result;
}

qsh_sensor::qsh_sensor(suid sensor_uid, sensor_wakeup_type wakeup):
    sensor(wakeup),
    _reset_requested(false),
    _pending_flush_requests(0),
    _sensor_uid(sensor_uid),
    _attributes(lookup_attributes())
{
    set_wakeup_flag(wakeup == SENSOR_WAKEUP);
    _wakeup_type = wakeup;
    is_resolution_set = false;
    is_max_range_set = false;
    _notify_resp = false;
    _resp_ret = 0;
    _oneshot_worker = nullptr;
    _diag = nullptr;
    _qmiSession = nullptr;
    _stats = nullptr;
    _wakelock_inst = sns_wakelock::get_instance();
    set_sensor_info();
}

void qsh_sensor::set_sensor_typename(const string& type_name)
{
    string name_attr = attributes().get_string(SNS_STD_SENSOR_ATTRID_NAME);
    name_attr.pop_back(); /* remove NUL terminator */

    _sensor_name = name_attr + " " + type_name +
        ((_wakeup_type == SENSOR_WAKEUP) ? " Wakeup" : " Non-wakeup");
    set_name(_sensor_name.c_str());
}
void qsh_sensor::set_nowk_msgid(const int msg_id)
{
  _nowk_msg_id_list.push_back(msg_id);
}
void qsh_sensor::set_sensor_info()
{
    const sensor_attributes& attr = attributes();

    set_vendor(attr.get_string(SNS_STD_SENSOR_ATTRID_VENDOR).c_str());
    _datatype = attr.get_string(SNS_STD_SENSOR_ATTRID_TYPE);

    set_sensor_typename();

    auto version = attr.get_ints(SNS_STD_SENSOR_ATTRID_VERSION);
    if (version.size() > 0 && version[0] > 0) {
        set_version(version[0]);
    } else {
        set_version(1);
    }

    auto stream_type = attr.get_stream_type();
    switch (stream_type) {
      case SNS_STD_SENSOR_STREAM_TYPE_STREAMING: {
          set_reporting_mode(SENSOR_FLAG_CONTINUOUS_MODE);
          vector<float> rates = attr.get_floats(SNS_STD_SENSOR_ATTRID_RATES);
          /* streaming sensors must have rates attribute */
          if (rates.size() == 0) {
              throw runtime_error(
                  "rates attribute unavailable for a streaming sensor");
          }

          /* ceil() is used to make sure that delay (period) is not
             truncated, as it will cause the calculated rates to be
             more than what is supported by the sensor */
          _sensor_max_delay = int32_t(ceil(USEC_PER_SEC / rates[0]));
          set_max_delay(_sensor_max_delay);
          int32_t min_delay = int32_t(ceil(USEC_PER_SEC / rates[rates.size()-1]));
          set_min_delay(min_delay);

          vector<float> low_lat_rates = attr.get_floats(SNS_STD_SENSOR_ATTRID_ADDITIONAL_LOW_LATENCY_RATES);
          if (low_lat_rates.size() != 0) {
            min_delay = int32_t(ceil(USEC_PER_SEC / low_lat_rates[low_lat_rates.size()-1]));
          }
          set_min_low_lat_delay(min_delay);

          /* if streaming sensor is physical, use resampler */
          auto phys_sensor_attr = attr.get_bools(
              SNS_STD_SENSOR_ATTRID_PHYSICAL_SENSOR);
          if (phys_sensor_attr.size() > 0 && phys_sensor_attr[0] == true) {
              set_resampling(true);
              set_nowk_msgid(SNS_STD_MSGID_SNS_STD_RESAMPLER_CONFIG_EVENT);
          }
          break;
      }
      case SNS_STD_SENSOR_STREAM_TYPE_ON_CHANGE:
      case SNS_STD_SENSOR_STREAM_TYPE_SINGLE_OUTPUT:
          set_reporting_mode(SENSOR_FLAG_ON_CHANGE_MODE);
          break;
      default:
          throw runtime_error("invalid stream_type " + to_string(stream_type));
    }

    auto resolution = attr.get_floats(SNS_STD_SENSOR_ATTRID_SELECTED_RESOLUTION);
    if (resolution.size() > 0) {
        set_resolution(resolution[0]);
        is_resolution_set = true;
    } else {
        /* set default value 0.1 */
        set_resolution(0.01f);
        sns_logd("dt=%s, SNS_STD_SENSOR_ATTRID_SELECTED_RESOLUTION not set",
                 _datatype.c_str());
    }


    auto ranges = attr.get_ranges(SNS_STD_SENSOR_ATTRID_SELECTED_RANGE);
    if (ranges.size() > 0) {
        set_max_range(ranges[0].second);
        is_max_range_set = true;
    } else {
        /* set default value 1.0 */
        set_max_range(1.0f);
        sns_logd("dt=%s, SNS_STD_SENSOR_ATTRID_SELECTED_RANGE not set",
                 _datatype.c_str());
    }

    auto currents = attr.get_ints(SNS_STD_SENSOR_ATTRID_ACTIVE_CURRENT);
    if (currents.size() > 0) {
        /* use max current and convert from uA to mA */
        auto max_iter = std::max_element(currents.begin(), currents.end());
        set_power(float(*max_iter) / 1000.0);
    }

    // TODO: calculate following based on system capacity
    set_fifo_max_count(10000);

    auto incompatable_state = attr.get_bools(SNS_STD_SENSOR_ATTRID_HLOS_INCOMPATIBLE);
    if (incompatable_state.size() > 0) {
        set_incompatible_flag(incompatable_state[0]);
    } else {
        /*Set Default value as false - Means this sensor is compatible with HLOS*/
        set_incompatible_flag(false);
    }
}

void qsh_sensor::activate()
{
    std::lock_guard<mutex> lk(_mutex);
    _previous_ts = 0;
    std::memset(&_prev_sample, 0x00, sizeof(Event));
    if(_is_stats_enable){
      if(_stats == NULL){
        _stats = new qsh_sensor_stats();
      }
      _stats->start();
    }
    if(_is_one_shot_senor){
        _oneshot_done = false;
        if(nullptr == _oneshot_worker)
            _oneshot_worker = make_unique<worker>();
    }
    if (!is_active()) {
        if (_reset_requested) {
            sns_logi("reset was done. do suid discovery");
            std::chrono::steady_clock::time_point disc_start = steady_clock::now();
            bool sensor_available = is_sensor_available();
            if(!sensor_available){
              sns_loge("could not discover sensor");
              throw runtime_error("suid discovery failed");
            }
            _reset_requested = false;
            sns_logi("sensor is discovered. it takes %fs",
                duration_cast<duration<double>>(steady_clock::now() - disc_start).count());
        }
        _set_rt_prio_event_cb = true;

        acquire_qsh_interface_instance();

        string thread_name = "see_" + to_string(get_sensor_info().handle);

        try {
          start_stream();
        } catch (const exception& e) {
            sns_loge("failed: %s", e.what());
            release_qsh_interface_instance();
            throw;
        }
   }
   sns_logd("(%s) , _pending_flush_requests=%u , _wakeup_type=%u" ,_datatype.c_str(),(unsigned int) _pending_flush_requests, _wakeup_type);
}

void qsh_sensor::release_qsh_interface_instance()
{
  qsh_intf_resource_manager::get_instance().release(_qsh_intf_id, _qmiSession);
  _qmiSession = nullptr;
  if(nullptr != _diag) {
    delete _diag;
    _diag = nullptr;
  }
}

void qsh_sensor::acquire_qsh_interface_instance()
{

  ISession::eventCallBack event_cb =[this](const uint8_t* msg , int msgLength, uint64_t time_stamp)
                { qsh_conn_event_cb(msg, msgLength, time_stamp); };

  ISession::respCallBack resp_cb=[this](uint32_t resp_value, uint64_t client_connect_id)
          { qsh_conn_resp_cb(resp_value, client_connect_id); };

  ISession::errorCallBack error_cb =[this](ISession::error error)
                { qsh_conn_error_cb(error); };

  _qmiSession = qsh_intf_resource_manager::get_instance().acquire(_qsh_intf_id);
  if(nullptr == _qmiSession) {
    sns_loge("Isession create failed");
    return;
  }
  int ret = _qmiSession->setCallBacks(_sensor_uid,resp_cb,error_cb,event_cb);
  if(0 != ret)
    printf("all callbacks are null, not registered any callback for suid(low=%" PRIu64 ",high=%)" PRIu64, _sensor_uid.low, _sensor_uid.high);

  if(nullptr == _diag) {
    _diag = new sensors_diag_log();
    if(nullptr == _diag)
      sns_loge("diag memory creation failed");
  }
}

void qsh_sensor::start_stream()
{
  try {
      send_sensor_config_request();
      _set_thread_name = true;
  } catch (const exception& e) {
    sns_loge("can't activate sensor because %s", e.what());
    throw;
  }
}
void qsh_sensor::send_sensor_disable_request()
{
  sns_client_request_msg pb_req_msg;
  string pb_req_msg_encoded;

  pb_req_msg.set_msg_id(SNS_CLIENT_MSGID_SNS_CLIENT_DISABLE_REQ);

  pb_req_msg.mutable_suid()->set_suid_high(_sensor_uid.high);
  pb_req_msg.mutable_suid()->set_suid_low(_sensor_uid.low);
  pb_req_msg.mutable_susp_config()->set_delivery_type(get_delivery_type());
  pb_req_msg.mutable_susp_config()->set_client_proc_type(SNS_STD_CLIENT_PROCESSOR_APSS);
  pb_req_msg.mutable_request()->mutable_batching()->set_flush_period(UINT32_MAX);
  pb_req_msg.mutable_request()->mutable_batching()->set_batch_period(0);
  pb_req_msg.mutable_request()->clear_payload();
  pb_req_msg.set_client_tech(SNS_TECH_SENSORS);

  pb_req_msg.SerializeToString(&pb_req_msg_encoded);

  if(nullptr != _diag) {
    _diag->log_request_msg(pb_req_msg_encoded, _datatype, DIAG_LOG_MODULE_NAME);
  }
  send_sync_sensor_request(pb_req_msg_encoded);

}

void qsh_sensor::deactivate()
{
    std::lock_guard<mutex> lk(_mutex);
    auto release_resources = [this](){
      release_qsh_interface_instance();
      _is_deactivate_in_progress = false;
      _pending_flush_requests = 0;
      if(_stats != NULL){
        _stats->stop();
        sns_logi("Sensor (%s) : %s ", _datatype.c_str(), _stats->get_stats().c_str());
      }
    };

    if (is_active()) {
      _is_deactivate_in_progress = true;
      try {
        send_sensor_disable_request();
      }
      catch (const exception& e) {
        sns_loge("sending disable request failed");
        release_resources();
        throw;
      }
      sns_logd("(%s) , _pending_flush_requests=%u , _wakeup_type=%u" ,_datatype.c_str(),(unsigned int) _pending_flush_requests, _wakeup_type);
      release_resources();
    }
    else{
      sns_logi("(%s) is not active, no need to deactivate.", _datatype.c_str());
    }
}

void qsh_sensor::add_nowk_msgid_list(sns_client_request_msg &req_msg)
{
  std::vector<int>::iterator it = _nowk_msg_id_list.begin();
  while(it != _nowk_msg_id_list.end())
  {
    req_msg.mutable_susp_config()->add_nowakeup_msg_ids(*it);
    it++;
  }
}

void qsh_sensor::send_sync_sensor_request(string& pb_req_msg_encoded)
{

    std::unique_lock<std::recursive_mutex> lk(_resp_mutex);
    sns_logi("send sync request");
    _resp_ret = 0;
    _resp_received = false;
    _notify_resp = false;
    if (!is_active()) {
        sns_loge("qsh connection is reset");
        return;
    }
    sns_logd("%s before send_request for %s with SUID low=%llu high=%llu",
        __func__, _datatype.c_str(),
        (unsigned long long)_sensor_uid.low, (unsigned long long)_sensor_uid.high);
    int ret = _qmiSession->sendRequest(_sensor_uid, pb_req_msg_encoded);
    if(0 != ret){
        printf("Error in sending request");
        return;
    }
    if (_resp_received) {
        // there is a case where response callback is called within the same context
        // of calling send_request - according to QMI team, it's the case where
        // error callback is called because of TX buffer is full
        // in this case, we shouldn't call wait_for of conditional variable instead,
        // return this function immediately based on response result
        sns_logi("resp_cb is called as a part of send_requet, This is not normal case");
        // trigger SSR for debugging purpose
        trigger_ssr();
        if (_resp_ret != 0) {
            sns_loge("config sensor fails, error = %u", _resp_ret);
            throw runtime_error("config sensor fails");
        }
    } else {
        // in reconnecting case, send_request can return without sending actual request
        // however, since send_request is a void function, there is no return value to indicate that case
        // so that, we can't catch it.
        sns_logi("wait for notification of response");
        _notify_resp = true;
        auto start_ts = steady_clock::now();
        bool cv_ret = _resp_cv.wait_for(lk, std::chrono::milliseconds(_resp_timeout_msec), [this]{ return _resp_received; });
        if (cv_ret) {
            sns_logi("takes %ld ms to receive the response with %u",
                duration_cast<duration<long, std::milli>>(steady_clock::now() - start_ts).count(), _resp_ret);
            if (_resp_ret != 0) {
                sns_loge("config sensor fails, error = %u", _resp_ret);
                throw runtime_error("config sensor fails");
            }
        } else {
            sns_loge("response doesn't come within %u msec", _resp_timeout_msec);
            throw runtime_error("config sensor timeout");
        }
    }
}

void qsh_sensor::send_sensor_config_request()
{
    string pb_req_msg_encoded;

    sns_client_request_msg pb_req_msg;
    pb_req_msg = create_sensor_config_request();
    pb_req_msg.mutable_suid()->set_suid_high(_sensor_uid.high);
    pb_req_msg.mutable_suid()->set_suid_low(_sensor_uid.low);

    if (_resampling == true) {
        sns_logi("resampler is used, set resampler config");
        pb_req_msg.mutable_resampler_config()->set_rate_type(SNS_RESAMPLER_RATE_MINIMUM);
        pb_req_msg.mutable_resampler_config()->set_filter(true);
    }

    uint32_t batch_period_us = get_params().max_latency_ns / NSEC_PER_USEC;
    uint32_t flush_period_us = 0;

    /* To support Android Hi-Fi requirements for non-wakeup sensors, if
       batch period is less than the time required to buffer
       fifoReservedEventCount number of events, we need to set flush period
       according to the fifoReservedEventCount. For streaming sensors,
       this will be derived from the sample_period and for non-streaming
       sensors, flush period will be set to a max value */
    if (_wakeup_type == SENSOR_NO_WAKEUP) {
        if (get_reporting_mode() == SENSOR_FLAG_CONTINUOUS_MODE) {
            uint32_t batch_period_max_us =
            (get_params().sample_period_ns / NSEC_PER_USEC) *
            get_sensor_info().fifoMaxEventCount;
            if(batch_period_us > batch_period_max_us) {
                sns_logi("dt=%s, asked batch_period_us : %u adjusted_us : %u",
                    _datatype.c_str(),
                    (unsigned int)batch_period_us,
                    (unsigned int)batch_period_max_us);
                batch_period_us = batch_period_max_us;
            }
         }
        if (get_reporting_mode() == SENSOR_FLAG_CONTINUOUS_MODE)
        {
            flush_period_us = (get_params().sample_period_ns / NSEC_PER_USEC) *
                    get_sensor_info().fifoReservedEventCount;
        } else {
            if (get_sensor_info().fifoReservedEventCount > 0) {
                flush_period_us = UINT32_MAX;
            }
        }
        if (flush_period_us > batch_period_us) {
            pb_req_msg.mutable_request()->mutable_batching()->
                set_flush_period(flush_period_us);
        } else {
            flush_period_us = 0;
        }
    } else {
        /*after fifoMaxEventCount need to wake up Apps and send the samples
        set batch period corresponding to that as you need to wakeup the App*/
        if (get_reporting_mode() == SENSOR_FLAG_CONTINUOUS_MODE) {
            uint32_t batch_period_max_us =
            (get_params().sample_period_ns / NSEC_PER_USEC) *
            get_sensor_info().fifoMaxEventCount;
            if(batch_period_us > batch_period_max_us) {
                sns_logi("dt=%s, asked batch_period_us : %u adjusted_us : %u",
                    _datatype.c_str(),
                    (unsigned int)batch_period_us,
                    (unsigned int)batch_period_max_us);
                batch_period_us = batch_period_max_us;
            }
         }
    }

    if (get_reporting_mode() == SENSOR_FLAG_ON_CHANGE_MODE) {
        uint32_t calcuated_batch_period=0;
        uint64_t sample_period_us = get_params().sample_period_ns/NSEC_PER_USEC;
        if (sample_period_us > UINT32_MAX) {
            sns_logi("sample_period_us is larger than acceptable value");
            calcuated_batch_period= UINT32_MAX;
        } else {
            calcuated_batch_period = sample_period_us;
        }
        batch_period_us= (batch_period_us > calcuated_batch_period)? batch_period_us : calcuated_batch_period;
        sns_logi("dt=%s: convert sample_period=%" PRIu64 " to batch_period=%u",
             _datatype.c_str(), get_params().sample_period_ns, batch_period_us);
    }

    pb_req_msg.mutable_request()->mutable_batching()->
        set_batch_period(batch_period_us);

    pb_req_msg.mutable_susp_config()->set_delivery_type(get_delivery_type());
    pb_req_msg.mutable_susp_config()->
        set_client_proc_type(SNS_STD_CLIENT_PROCESSOR_APSS);

    pb_req_msg.set_client_tech(SNS_TECH_SENSORS);

    pb_req_msg.SerializeToString(&pb_req_msg_encoded);

    sns_logd("dt=%s, bp=%u, fp=%u, resampling=%d", _datatype.c_str(),
             (unsigned int) batch_period_us,
             (unsigned int) flush_period_us,
             _resampling);
    if(nullptr != _diag) {
        _diag->log_request_msg(pb_req_msg_encoded, _datatype, DIAG_LOG_MODULE_NAME);
    }
    send_sync_sensor_request(pb_req_msg_encoded);

}

void qsh_sensor::update_config()
{
    lock_guard<mutex> lk(_mutex);
    send_sensor_config_request();
}

void qsh_sensor::update_handle(int type)
{
    int hwid = 0;
    if (_attributes.is_present(SNS_STD_SENSOR_ATTRID_HW_ID)) {
        hwid = _attributes.get_ints(SNS_STD_SENSOR_ATTRID_HW_ID)[0];
    }

    int base = 0;
    if (type < SENSOR_TYPE_DEVICE_PRIVATE_BASE) {
      base = type*SENSOR_HANDLE_RANGE + 1;
    } else if (type >= QTI_SENSOR_TYPE_BASE) {
      base = QTI_SENSOR_TYPE_HANDLE_BASE + (type - QTI_SENSOR_TYPE_BASE)*SENSOR_HANDLE_RANGE + 1;
    }

    int handle = base + 2*hwid + _wakeup_type;
    sns_logd("handle=%d, type=%d, wakeup=%d, hwid=%d, base=%d", handle, type, _wakeup_type, hwid, base);
    set_handle(handle);
}

void qsh_sensor::flush()
{
    std::lock_guard<mutex> lk(_mutex);
    if (!is_active()) {
        return;
    }
    sns_client_request_msg pb_req_msg;
    string pb_req_msg_encoded;

    pb_req_msg.set_msg_id(SNS_STD_MSGID_SNS_STD_FLUSH_REQ);
    pb_req_msg.mutable_request()->clear_payload();
    pb_req_msg.mutable_suid()->set_suid_high(_sensor_uid.high);
    pb_req_msg.mutable_suid()->set_suid_low(_sensor_uid.low);

    pb_req_msg.mutable_susp_config()->set_delivery_type(get_delivery_type());
    pb_req_msg.mutable_susp_config()->set_client_proc_type(SNS_STD_CLIENT_PROCESSOR_APSS);
    pb_req_msg.set_client_tech(SNS_TECH_SENSORS);
    pb_req_msg.SerializeToString(&pb_req_msg_encoded);

    _pending_flush_requests++;
    unsigned int pending = _pending_flush_requests;
    sns_logd("dt=%s, sending SNS_STD_MSGID_SNS_STD_FLUSH_REQ, _pending_flush_requests=%u, _wakeup_type=%u",
             _datatype.c_str(),(unsigned int) pending, _wakeup_type);

    if(nullptr != _diag) {
        _diag->log_request_msg(pb_req_msg_encoded, _datatype, DIAG_LOG_MODULE_NAME);
    }
    send_sync_sensor_request(pb_req_msg_encoded);

}

void qsh_sensor::handle_sns_std_flush_event(
    uint64_t ts)
{
    unsigned int pending = _pending_flush_requests;
    if (pending > 0) {
        Event hal_event;
        memset(&hal_event, 0x00, sizeof(Event));
        hal_event.sensorType = SensorType::META_DATA;
        hal_event.u.meta.what = MetaDataEventType::META_DATA_FLUSH_COMPLETE;
        hal_event.sensorHandle = get_sensor_info().handle;
        hal_event.timestamp = sensors_timeutil::get_instance().
                        qtimer_ticks_to_elapsedRealtimeNano(ts);
        sns_logd("dt=%s, META_DATA_FLUSH_COMPLETE, _pending_flush_requests=%u, _wakeup_type=%u" ,
                 _datatype.c_str(), (unsigned int) pending, _wakeup_type);

        if(true == _is_stats_enable) {
            sns_loge("(%s) flush latency %lf", _datatype.c_str(),
                (double)(android::elapsedRealtimeNano()-ts)/NSEC_PER_MSEC);
        }
        events.push_back(hal_event);
        _pending_flush_requests--;
        pending = _pending_flush_requests;
    }
}

void qsh_sensor::qsh_conn_event_cb(const uint8_t *data, size_t size, uint64_t sample_received_ts)
{
    SNS_TRACE_BEGIN("sensors::qsh_conn_event_cb");

    if(_set_rt_prio_event_cb) {
      _set_rt_prio_event_cb = false;
      struct sched_param param = {0};
      param.sched_priority = _sched_priority;
      pid_t pid_self = gettid();
      if(sched_setscheduler(pid_self, SCHED_FIFO, &param) != 0) {
        sns_loge("could not set SCHED_FIFO for qmi_cbk RT Thread");
      } else {
        sns_logi("SCHED_FIFO(10) for qmi_cbk");
      }
    }

    if (_set_thread_name == true) {
        _set_thread_name = false;
        int ret_code = 0;
        string pthread_name = to_string(get_sensor_info().handle) + "_see";
        ret_code = pthread_setname_np(pthread_self(), pthread_name.c_str());
        if (ret_code != 0) {
            sns_loge("Failed to set ThreadName: %s\n", pthread_name.c_str());
        }
    }

    int sample_count = 0;
    if(SENSOR_WAKEUP == _wakeup_type && nullptr !=_wakelock_inst)
      _wakelock_inst->get_n_locks(1);

    bool flush_req = false;
    if(_stats != NULL)
      _stats->add_sample(sample_received_ts);
    string pb_event_msg_encoded((char *)data, size);
    if(nullptr != _diag) {
        _diag->log_event_msg(pb_event_msg_encoded, _datatype, DIAG_LOG_MODULE_NAME);
    }

    sns_client_event_msg pb_event_msg;
    pb_event_msg.ParseFromArray(data, size);
    for (sample_count=0; sample_count < pb_event_msg.events_size(); sample_count++) {
        auto&& pb_event = pb_event_msg.events(sample_count);
        sns_logv("event[%d] msg_id=%d, ts=%llu", sample_count, pb_event.msg_id(),
                 (unsigned long long) pb_event.timestamp());
        if (pb_event.msg_id() == SNS_STD_MSGID_SNS_STD_FLUSH_EVENT && _donot_honor_flushevent == false) {
            handle_sns_std_flush_event(pb_event.timestamp());
            flush_req = true;
        } else {
            handle_sns_client_event(pb_event);
        }
    }

    if (events.size()) {
        _event_count = events.size();
        submit_sensors_hal_event();
        if(_is_one_shot_senor){
          sns_logd(" one_shot sensor(%s) sample submitted! ", _datatype.c_str());
          _oneshot_worker->setname(_datatype.c_str());
          _oneshot_done = true;
          _oneshot_worker->add_task([this] {
            sns_logd(" deactivating one_shot_sensor(%s)...", _datatype.c_str());
            qsh_sensor::deactivate();
          });
          }
        }

    if (flush_req) {
        on_flush_complete();
    }
    if(SENSOR_WAKEUP == _wakeup_type && nullptr !=_wakelock_inst)
      _wakelock_inst->put_n_locks(1);
    SNS_TRACE_END();
}
void qsh_sensor::qsh_conn_resp_cb(uint32_t resp_value, uint64_t client_connect_id)
{
    sns_logi("resp_value = %u for %s ", resp_value, datatype().c_str());

    std::unique_lock<std::recursive_mutex> lk(_resp_mutex);
    _resp_received = true;
    _resp_ret = resp_value;
    if (_notify_resp) {
         _resp_cv.notify_one();
    } else {
         sns_logi("no need to notify");
    }

    if(nullptr != _diag) {
      _diag->update_client_connect_id(client_connect_id);
      _diag->log_response_msg(resp_value,_datatype, DIAG_LOG_MODULE_NAME);
    }
}
bool qsh_sensor::is_sensor_available()
{
  sns_logi("start discovery process for %s/%d", _datatype.c_str(), get_sensor_info().handle);

  bool got_current_suid = false;
  std::condition_variable suid_cv;
  std::mutex suid_mutex;

  /*suid_lookup for actual sensor*/
  unique_ptr<suid_lookup> lookup = make_unique<suid_lookup>([&](const string& datatype, const auto& suids){
                         std::unique_lock<std::mutex> lock(suid_mutex);
                         for (size_t i = 0; i < suids.size(); i++) {
                            if ((suids[i].high == _sensor_uid.high) && (suids[i].low == _sensor_uid.low)) {
                              got_current_suid = true;
                              suid_cv.notify_one();
                              sns_logi("got the suid for %s/%d", datatype.c_str(), get_sensor_info().handle);
                            }
                         }
                      });

  //check for suid and then send config request
  std::unique_lock<std::mutex> lock(suid_mutex);
  lookup->request_suid(_datatype, true);
  if(!suid_cv.wait_for(lock, std::chrono::milliseconds(SUID_DISCOVERY_TIMEOUT_MS),
      [&got_current_suid]{ return got_current_suid; })){
    sns_logi("Suid lookup for %s/%d timeout !", _datatype.c_str(), get_sensor_info().handle);
  }
  lock.unlock();
  lookup.reset();
  sns_logi("discovery process is completed for %s/%d with result = %d", _datatype.c_str(), get_sensor_info().handle, got_current_suid);
  return got_current_suid;
}

void qsh_sensor::qsh_conn_error_cb(ISession::error e)
{
  if(true == _is_deactivate_in_progress){
    sns_logi("%s is being deactivated.", _datatype.c_str());
    return;
  }
  sns_loge("handle error: %d for %s", e, _datatype.c_str());
  /* re-send config request when connection resets */
  bool got_sensorback = is_sensor_available();
  if (is_active()) {
    if (got_sensorback){
      try{
        send_sensor_config_request();
      }catch(const exception& excp){
        sns_loge("sending sync request failed in error_cb for %s because %s", _datatype.c_str(), excp.what());
      }
    }
    else
      sns_loge("could not restart %s after ssr", _datatype.c_str());
  } else {
    sns_logi("%s deactivated during ssr", _datatype.c_str());
  }
  _reset_requested = false;
}

void qsh_sensor::handle_sns_client_event(
        const sns_client_event_msg_sns_client_event& pb_event)
{
    switch(pb_event.msg_id()) {
      case SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_EVENT:
          handle_sns_std_sensor_event(pb_event);
          break;
      default:
          break;
    }
}

sns_client_request_msg qsh_sensor::create_sensor_config_request()
{
    sns_client_request_msg pb_req_msg;
    auto stream_type = attributes().get_stream_type();
    add_nowk_msgid_list(pb_req_msg);
    /* populate request message based on stream type */
    if (stream_type == SNS_STD_SENSOR_STREAM_TYPE_STREAMING) {
        float sample_rate = float(NSEC_PER_SEC) / get_params().sample_period_ns;
        sns_logd("sr=%f", sample_rate);
        sns_std_sensor_config pb_stream_cfg;
        string pb_stream_cfg_encoded;
        sns_logd("sending SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_CONFIG");
        pb_stream_cfg.set_sample_rate(sample_rate);
        pb_stream_cfg.SerializeToString(&pb_stream_cfg_encoded);
        pb_req_msg.set_msg_id(SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_CONFIG);
        pb_req_msg.mutable_request()->set_payload(pb_stream_cfg_encoded);
    } else {
        sns_logd("sending SNS_STD_SENSOR_MSGID_SNS_STD_ON_CHANGE_CONFIG");
        pb_req_msg.set_msg_id(SNS_STD_SENSOR_MSGID_SNS_STD_ON_CHANGE_CONFIG);
        pb_req_msg.mutable_request()->clear_payload();
    }
    return pb_req_msg;
}


bool qsh_sensor::duplicate_onchange_sample(const Event& hal_event)
{
    bool duplicate_sample = false;
    auto stream_type = attributes().get_stream_type();
    if (stream_type == SNS_STD_SENSOR_STREAM_TYPE_ON_CHANGE) {
        if (std::memcmp(&hal_event, &_prev_sample, sizeof(Event)) == 0) {
            duplicate_sample = true;
        }
        _prev_sample = hal_event;
    }
    return duplicate_sample;
}

void qsh_sensor::handle_sns_std_sensor_event(
    const sns_client_event_msg_sns_client_event& pb_event)
{
    SNS_TRACE_BEGIN("sensors::handle_sns_std_sensor_event");
    sns_std_sensor_event pb_stream_event;
    pb_stream_event.ParseFromString(pb_event.payload());

    Event hal_event = create_sensor_hal_event(pb_event.timestamp());

    int num_items = pb_stream_event.data_size();
    if (num_items > SENSORS_HAL_MAX_DATA_LEN) {
        sns_loge("num_items=%d exceeds SENSORS_HAL_MAX_DATA_LEN=%d",
                 num_items, SENSORS_HAL_MAX_DATA_LEN);
        num_items = SENSORS_HAL_MAX_DATA_LEN;
    }

    for (int i = 0; i < num_items; i++) {
        hal_event.u.data[i] = pb_stream_event.data(i);
    }

    if (sensors_log::get_level() >= sensors_log::DEBUG) {
        /* debug: print the event data */
        string s = "[";
        for (int i = 0; i < pb_stream_event.data_size(); i++) {
            s += to_string(pb_stream_event.data(i));
            if (i < pb_stream_event.data_size() - 1) {
                 s += ", ";
            } else {
                s += "]";
            }
        }
        sns_logd("%s_sample: ts = %llu ns; value = %s", _datatype.c_str(),
                 (unsigned long long) hal_event.timestamp, s.c_str());
    }

    if(_stats != NULL)
      _stats->update_sample(hal_event.timestamp, pb_event.timestamp());

    if(true == can_submit_sample(hal_event))
      events.push_back(hal_event);

    SNS_TRACE_END();
}

Event qsh_sensor::create_sensor_hal_event(uint64_t qsh_timestamp)
{
    Event hal_event;
    //Fix for static analysis error - uninitialized variable
    memset(&hal_event, 0x00, sizeof(Event));
    hal_event.sensorHandle = get_sensor_info().handle;
    hal_event.sensorType = (SensorType)get_sensor_info().type;
    hal_event.timestamp = sensors_timeutil::get_instance().
        qtimer_ticks_to_elapsedRealtimeNano(qsh_timestamp);

    sns_logv("qsh_ts = %llu tk, hal_ts = %lld ns, %lf",
             (unsigned long long)qsh_timestamp,
             (long long)hal_event.timestamp, hal_event.timestamp / 1000000000.0 );
    return hal_event;
}

void qsh_sensor::set_resampling(bool val)
{
    _resampling = val;
    /* change max delay (min rate) based on resampler usage */
    set_max_delay(val ? RESAMPLER_MAX_DELAY : _sensor_max_delay);
}

void qsh_sensor::donot_honor_flushevent(bool val)
{
    _donot_honor_flushevent = val;
}

SensorStatus qsh_sensor::sensors_hal_sample_status(
    sns_std_sensor_sample_status std_status)
{
    switch (std_status) {
      case SNS_STD_SENSOR_SAMPLE_STATUS_UNRELIABLE:
          return SensorStatus::UNRELIABLE;
      case SNS_STD_SENSOR_SAMPLE_STATUS_ACCURACY_LOW:
          return SensorStatus::ACCURACY_LOW;
      case SNS_STD_SENSOR_SAMPLE_STATUS_ACCURACY_MEDIUM:
          return SensorStatus::ACCURACY_MEDIUM;
      case SNS_STD_SENSOR_SAMPLE_STATUS_ACCURACY_HIGH:
          return SensorStatus::ACCURACY_HIGH;
      default:
          sns_loge("invalid std_status = %d", std_status);
    }
    return SensorStatus::UNRELIABLE;
}

static bool qsh_sensor_module_init()
{
    return true;
}

SENSOR_MODULE_INIT(qsh_sensor_module_init);

/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "qsh_intf_resource_manager.h"
#include "suid_lookup.h"
#include "sensors_log.h"
#include "sns_cal.pb.h"
#include "sensor_calibration.h"

#include <android/hardware/sensors/1.0/types.h>
#include <android/hardware/sensors/2.1/types.h>
#include <hardware/sensors-base.h>

char DIAG_LOG_MODULE_NAME[] = "sensors-hal";
const auto SENSOR_RECOVERY_TIMEOUT_MS = 5000;
#define QSH_SENSORS_RESP_TIMEOUT_MS (500)

using namespace std;
using namespace chrono;

calibration::calibration(suid sensor_uid,string datatype,unsigned int qsh_intf_id) :
     _sensor_uid(sensor_uid),
     _datatype(datatype),
     _qmiSession(nullptr),
     _worker(nullptr),
     _sample_status(SNS_STD_SENSOR_SAMPLE_STATUS_UNRELIABLE),
     _resp_received(false),
     _qsh_intf_id(qsh_intf_id)
{

  _event_cb =[this](const uint8_t* msg , size_t msgLength, uint64_t time_stamp)
                              { handle_event_cb(msg, msgLength, time_stamp); };

  _resp_cb=[this](uint32_t resp_value, uint64_t client_connect_id)
                        { handle_resp_cb(resp_value, client_connect_id); };
}

calibration::~calibration()
{
  _biases.clear();
}

void calibration::reactivate()
{
  /* re-send cal request when connection resets */
  mutex suid_mutex;
  condition_variable suid_cv;
  bool got_sensorback = false;
  sns_logi("connection reset, resend cal request");

  if (!_is_active) {
    sns_loge("%s was not activated", _datatype.c_str());
    throw runtime_error("cal sensor was not activated");
  }

  suid_lookup lookup([&](const string& datatype, const auto& suids){
                      sns_logd("got the suid call back");
                      unique_lock<mutex> lock(suid_mutex);
                      if(0 != suids.size()) {
                        _sensor_uid = suids[0];
                        got_sensorback = true;
                        suid_cv.notify_one();
                      }
                    });

  //check for suid and then send config request
  unique_lock<mutex> lock(suid_mutex);
  lookup.request_suid(_datatype, true);
  if (suid_cv.wait_for(lock, chrono::milliseconds(SENSOR_RECOVERY_TIMEOUT_MS)) == cv_status::timeout) {
    sns_loge("cal request after ssr - timed out!!");
  }

  if (got_sensorback) {
    if (!send_req(true)) {
      sns_loge("fail to send config request for %s, fail to reactivate", _datatype.c_str());
      throw runtime_error("connection closed");
    }
  } else {
    sns_loge("%s is not discovered after ssr. fail to reactivate", _datatype.c_str());
    throw runtime_error("can't discover the sensor");
  }
}

calibration::bias calibration::get_bias(uint64_t timestamp)
{

  if (_biases.size() == 0) {
    bias b;
    b.values[0] = 0.0f;
    b.values[1] = 0.0f;
    b.values[2] = 0.0f;
    b.timestamp = 0;
    b.status = SNS_STD_SENSOR_SAMPLE_STATUS_UNRELIABLE;
    return b;
  }

  auto cur= _biases.begin();
  auto prev = cur;
  cur++;

  while (cur != _biases.end()) {
    if (cur->timestamp <= timestamp) {
      _biases.erase(prev);
    } else {
      break;
    }
    prev = cur;
    cur++;
  }
  return *prev;
}

void calibration::activate()
{
  if (_is_active) {
    sns_loge("%s was already activated", _datatype.c_str());
    throw runtime_error("cal sensor was already activated");
  }
  auto release_resources = [this] {
    if (nullptr != _worker) {
      _worker.reset();
      _worker = nullptr;
    }
    if (nullptr != _qmiSession) {
      int ret = _qmiSession->setCallBacks(_sensor_uid, nullptr, nullptr, nullptr);
      if(0 == ret)
        sns_loge("unregistered all callbacks for %s", _datatype.c_str());
      qsh_intf_resource_manager::get_instance().release(_qsh_intf_id, _qmiSession);
      _qmiSession = nullptr;
    }
    if (nullptr != _diag) {
      _diag.reset();
      _diag = nullptr;
    }
  };

  _worker = make_unique<worker>();
  if (nullptr != _worker)
    sns_logd("created worker for %s",_datatype.c_str());
  else {
    sns_loge("failed to create worker for %s", _datatype.c_str());
    release_resources();
    throw runtime_error("worker creation failed");
  }

  _diag = make_unique<sensors_diag_log>();
  if (nullptr != _diag)
    sns_logd("created diag instance for %s", _datatype.c_str());
  else {
    release_resources();
    sns_loge("failed to create diag for %s", _datatype.c_str());
    throw runtime_error("diag creation failed");
  }

  _qmiSession = qsh_intf_resource_manager::get_instance().acquire(_qsh_intf_id);
  if (_qmiSession == nullptr) {
    sns_loge("qsh_intf_resource_manager acquire failed for %s", _datatype.c_str());
    release_resources();
    throw runtime_error("connection acquisition failed");
  }

  sns_logd("acquired qmi conn for %s", _datatype.c_str());
  int ret = _qmiSession->setCallBacks(_sensor_uid,_resp_cb,nullptr,_event_cb);
  if(0 != ret)
    sns_loge("all callbacks are null, no need to register it");

  sns_logi("registered callback for %s", _datatype.c_str());

  if (!send_req(true)) {
    sns_loge("sending request failed for %s", _datatype.c_str());
    release_resources();
    throw runtime_error("sending request failed");
  }
  _is_active = true;
}

void calibration::deactivate()
{
  if (!_is_active) {
    sns_loge("%s was not activated", _datatype.c_str());
    return;
  }
  int ret = _qmiSession->setCallBacks(_sensor_uid,_resp_cb,nullptr,nullptr);
  if(0 != ret){
    sns_loge("all callbacks are null, no need to register it");
  }
  _worker.reset();
  _worker = nullptr;

  unique_lock<recursive_mutex> lock(_resp_mutex);
  sns_logd("acquired _resp_mutex");
  _resp_received = false;
  _is_disable_in_progress = false;
  if (send_req(false)) {
    if (!_resp_received) {
      sns_logi("sent disable request. wait for response for it");
      _is_disable_in_progress = true;
      bool result = _resp_cv.wait_for(lock, chrono::milliseconds(QSH_SENSORS_RESP_TIMEOUT_MS), [&] {
          sns_logi("resp_received = %d in wait_for", _resp_received);
          return _resp_received;
        });
      _is_disable_in_progress = false;
      if (!result)
        sns_loge("response doesn't come within %u msec", QSH_SENSORS_RESP_TIMEOUT_MS);
    } else {
      sns_logi("response comes within this context");
    }
  } else {
    //the case where send_req return fail is only when _qmiSession is nullptr
    //so that, we can do regular clean-up process
    sns_loge("sending request failed to deactivate calibration sensor");
  }
  lock.unlock();

  _diag.reset();
  _diag = nullptr;

  ret = _qmiSession->setCallBacks(_sensor_uid, nullptr, nullptr, nullptr);
  if(0 != ret)
    sns_loge("all callbacks are null, no need to register it");

  qsh_intf_resource_manager::get_instance().release(_qsh_intf_id, _qmiSession);
  _qmiSession = nullptr;
  sns_logd("released qmi conn for %s", _datatype.c_str());
  _is_active = false;
}

bool calibration::send_req(bool enable)
{
  lock_guard<mutex> lock(_request_mutex);
  string pb_req_msg_encoded;
  sns_client_request_msg pb_req_msg;

  if (enable) {
    pb_req_msg.set_msg_id(SNS_STD_SENSOR_MSGID_SNS_STD_ON_CHANGE_CONFIG);
  } else{
    pb_req_msg.set_msg_id(SNS_CLIENT_MSGID_SNS_CLIENT_DISABLE_REQ);
  }

  pb_req_msg.mutable_request()->clear_payload();
  pb_req_msg.mutable_suid()->set_suid_high(_sensor_uid.high);
  pb_req_msg.mutable_suid()->set_suid_low(_sensor_uid.low);

  /* If current calibration status is "UNRELIABLE", set wakeup flag to
   * ensure next valid calibration parameters are sent immediately. */
  pb_req_msg.mutable_request()->mutable_batching()->
      set_batch_period(0);
  pb_req_msg.mutable_request()->mutable_batching()->
      set_flush_period(UINT32_MAX);
  pb_req_msg.mutable_susp_config()->set_delivery_type(
      SENSOR_STATUS_UNRELIABLE != _sample_status
      ? SNS_CLIENT_DELIVERY_NO_WAKEUP : SNS_CLIENT_DELIVERY_WAKEUP);
  pb_req_msg.mutable_susp_config()->
      set_client_proc_type(SNS_STD_CLIENT_PROCESSOR_APSS);
  pb_req_msg.set_client_tech(SNS_TECH_SENSORS);

  pb_req_msg.SerializeToString(&pb_req_msg_encoded);

  _diag->log_request_msg(pb_req_msg_encoded, _datatype, DIAG_LOG_MODULE_NAME);

  int ret = _qmiSession->sendRequest(_sensor_uid, pb_req_msg_encoded);
  if(0 != ret) {
    sns_loge("fail to send request to %s (low=%" PRIu64 "high=%" PRIu64 ")", _datatype.c_str(), _sensor_uid.low, _sensor_uid.high);
    return false;
  }

  _request_count++;
  sns_logi("sent request to %s (low=%" PRIu64 "high=%" PRIu64 "), reqeuest_count = %u", _datatype.c_str(), _sensor_uid.low, _sensor_uid.high, _request_count.load());
  return true;
}

void calibration::handle_resp_cb(uint32_t resp_value, uint64_t client_connect_id)
{
  sns_logi("resp_value = %u for %s before acuiring the lock, pending request = %d", resp_value, _datatype.c_str(), _request_count.load());
  _request_count--;
  if (nullptr != _diag) {
    _diag->update_client_connect_id(client_connect_id);
    _diag->log_response_msg(resp_value,_datatype, DIAG_LOG_MODULE_NAME);
  }
  unique_lock<recursive_mutex> lock(_resp_mutex);
  sns_logd("handle_resp_cb: _request_count = %u",_request_count.load());
  _resp_received = true;
  if (true == _is_disable_in_progress && _request_count==0) {
    sns_logd("response received, disable is in progress, notify caller");
    _resp_cv.notify_one();
  } else {
    sns_logi("response received but, no need to notify caller, _is_disable_progress = %d, _request_count = %u",
      _is_disable_in_progress, _request_count.load());
  }
}

void calibration::handle_event_cb(const uint8_t *data, size_t size, uint64_t ts)
{
  sns_std_sensor_sample_status status;
  sns_client_event_msg pb_event_msg;
  pb_event_msg.ParseFromArray(data, size);

  string pb_event_msg_encoded((char *)data, size);
  if(nullptr != _diag)
    _diag->log_event_msg(pb_event_msg_encoded, _datatype, DIAG_LOG_MODULE_NAME);

  status = _sample_status;

  for (int i=0; i < pb_event_msg.events_size(); i++) {
    auto&& pb_event = pb_event_msg.events(i);
    sns_logv("event[%d] msg_id=%d", i, pb_event.msg_id());

    if (pb_event.msg_id() == SNS_CAL_MSGID_SNS_CAL_EVENT) {
      sns_cal_event pb_cal_event;
      pb_cal_event.ParseFromString(pb_event.payload());
      _sample_status = pb_cal_event.status();

      bias b;
      b.values[0] = pb_cal_event.bias(0);
      b.values[1] = pb_cal_event.bias(1);
      b.values[2] = pb_cal_event.bias(2);
      b.timestamp = pb_event.timestamp();
      b.status = _sample_status;
      _biases.push_back(b);

      sns_logd("%s bias=(%f, %f, %f) status=%d",
          _datatype.c_str(),b.values[0], b.values[1], b.values[2], (int)b.status);
    }
  }

  if ((SNS_STD_SENSOR_SAMPLE_STATUS_UNRELIABLE == status &&
      SNS_STD_SENSOR_SAMPLE_STATUS_UNRELIABLE != _sample_status) ||
     (SNS_STD_SENSOR_SAMPLE_STATUS_UNRELIABLE != status &&
      SNS_STD_SENSOR_SAMPLE_STATUS_UNRELIABLE == _sample_status)) {
      if(_worker != nullptr) {
        _worker->add_task([&]() {
            sns_logi("%s in worker.. sending cal request again", __func__);
            if (!send_req(true))
              sns_loge("Re-config request could not be sent for %s", _datatype.c_str());
        });
      }
  }
}
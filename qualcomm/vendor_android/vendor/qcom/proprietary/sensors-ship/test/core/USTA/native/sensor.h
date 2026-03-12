/*============================================================================
  Copyright (c) 2017-2018, 2020-2023 Qualcomm Technologies, Inc.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.

  @file sensor.h
  @brief
  Sensor class definition.
============================================================================*/
#pragma once

#include "ISession.h"
#include "sensor_common.h"
#include "sensor_attributes.h"
#include "sensor_message_parser.h"
#include "sensor_diag_log.h"
#include "data_stream.h"
#include <ctime>
#include <chrono>
#include <time.h>
#include <mutex>
#include <condition_variable>

using suid = com::quic::sensinghub::suid;

typedef enum usta_error_cb_type
{
  USTA_QMI_ERRRO_CB_FOR_ATTRIBUTES,
  USTA_QMI_ERRRO_CB_FOR_STREAM
}usta_error_cb_type;

typedef struct direct_channel_attributes_info
{
  float max_sampling_rate;
  float max_report_rate;
}direct_channel_attributes_info;

typedef struct sensor_info
{
  std::string data_type;
  std::string vendor;
  std::string suid_low;
  std::string suid_high;
}sensor_info;
/**
 * type alias for  event callback from Sensor for attribute request from client
 * This can be used by client as an acknowledgement that attribute has been
 * received
 */
typedef std::function<void(void)>handle_attribute_event_func;
using sensor_stream_event_cb = std::function<void(ISession* session, std::string sensor_event, void* sensor_instance)>;
using sensor_stream_error_cb = std::function<void(ISession::error stream_error)>;

/**
 * @brief all sensors must be instantiated with this class. The communication
 *        to QSH is via this class
 *
 */
class Sensor
{
public:
  Sensor(handle_attribute_event_func attrib_event_f,
      const uint64_t& suid_low,
      const uint64_t& suid_high,
      sensor_stream_event_cb push_event_cb,
      sensor_stream_error_cb push_error_cb
  );
  ~Sensor();
  usta_rc get_sensor_info(sensor_info& sensor_info_var);
  usta_rc send_attrib_request(ISession* session_attrb, sensors_diag_log* diag_attrb);
  usta_rc get_attributes(std::string& attribute_list);
  usta_rc get_request_list(std::vector<msg_info> &request_msgs);
  void get_payload_string(send_req_msg_info sensor_payload_field, std::string &encoded_data);

  int send_request(ISession *session,
      sensors_diag_log* diag,
      send_req_msg_info& formated_request,
      client_req_msg_fileds &std_fields_data,
      std::string& logfile_name);
private:
  handle_attribute_event_func attrib_event_cb;

  void sensor_attrib_event_cb(const uint8_t *encoded_data_string,
      size_t encoded_string_size,
      uint64_t time_stamp);
  void save_config_details(send_req_msg_info& formated_request,client_req_msg_fileds &std_fields_data,
      std::string& in_logfile_name , std::string &usta_mode);
  void save_payload_config(send_req_msg_info& formated_request);
  void save_standard_config(client_req_msg_fileds &std_fields_data);


  sensor_attribute* sensor_attribute_obj;
  MessageParser* sensor_parser;
  std::string attrib_json_string;

  void stream_event_cb(ISession* session, std::string sensor_event);
  void stream_error_cb(ISession::error stream_error);
  bool is_streaming(ISession *session);

  std::map<ISession* , data_stream*>    _data_stream_map_table;
  data_stream_event_cb                  _stream_event_cb;
  data_stream_error_cb                  _stream_error_cb;
  sensor_stream_event_cb                _push_event_cb = nullptr;
  sensor_stream_error_cb                _push_error_cb = nullptr;
  suid                                  _sensor_uid;
  worker*                               _cb_thread = nullptr;

  std::string                                _datatype;
  std::string                                _vendor;
  std::vector<std::string>                   _api;
  std::mutex                                 _sync_mutex;

  ISession*                             _session_attrb = nullptr;
  ISession::eventCallBack                          _attrb_event_cb = nullptr;
  ISession::respCallBack                           _attrb_resp_cb = nullptr;
  ISession::errorCallBack                          _attrb_error_cb = nullptr;
  sensors_diag_log*                     _diag_attrb = nullptr;
  bool                                  _is_session_cb_registered = false;
};

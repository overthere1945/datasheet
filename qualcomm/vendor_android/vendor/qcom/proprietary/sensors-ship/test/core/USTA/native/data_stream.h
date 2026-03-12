/*
 * Copyright (c) 2021-2025 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#pragma once
#include "sensor_message_parser.h"
#include "ISession.h"
#include "sensor_diag_log.h"
#include "worker.h"
#include <time.h>

using com::quic::sensinghub::session::V1_0::ISession;
using data_stream_event_cb = std::function<void(ISession* session, std::string stream_event)>;
using data_stream_error_cb = std::function<void(ISession::error stream_error)>;
using suid = com::quic::sensinghub::suid;

typedef struct client_req_msg_fileds
{
  std::string batch_period ;
  std::string flush_period ;
  std::string client_type ;
  std::string wakeup_type ;
  std::string flush_only ;
  std::string max_batch ;
  std::string is_passive ;
  std::string resampler_type ;
  std::string resampler_filter ;
  std::string threshold_type ;
  std::vector<std::string> threshold_value;
  std::vector<std::string> client_permissions;
}client_req_msg_fileds;

class data_stream {
public:
  data_stream(suid sensor_uid,
      std::string datatype,
      std::string vendor,
      std::string filename,
      std::vector<std::string> &api,
      data_stream_event_cb push_event_cb,
      data_stream_error_cb push_error_cb);
  ~data_stream();
  int send_request(ISession *session,
      sensors_diag_log* diag,
      send_req_msg_info& formated_request,
      client_req_msg_fileds &std_fields_data);

private:
  void stream_event_cb(const uint8_t *encoded_data_string,
      size_t encoded_string_size,
      uint64_t time_stamp);
  void handle_event(std::string  encoded_client_event_msg);
  void stream_resp_cb(uint32_t resp_value, uint64_t client_connect_id);
  void handle_resp(uint32_t resp_value, uint64_t client_connect_id);
  void stream_error_cb(ISession::error error_code);
  void handle_error(ISession::error error_code);
  usta_rc get_std_payload(send_req_msg_info& formated_request, std::string &std_payload);
  std::string get_error_name(uint32_t resp_value);
  void create_file(std::string filename);
  inline int get_client_connect_id() { return _client_connect_id; }
  inline void set_client_connect_id(int id) { _client_connect_id = id; }

  ISession*            _qmiSession = nullptr;
  sensors_diag_log*         _diag = nullptr;
  ISession::eventCallBack                  _stream_event_cb = nullptr;
  ISession::respCallBack                   _stream_resp_cb = nullptr;
  ISession::errorCallBack                  _stream_error_cb = nullptr;
  suid                      _sensor_uid;
  MessageParser*            _parser = nullptr;
  worker*                   _cb_thread = nullptr;
  data_stream_event_cb      _push_event_cb = nullptr;
  data_stream_error_cb      _push_error_cb = nullptr;

  int                       _client_connect_id = -1;
  bool                      _is_conn_cb_registered = false;
  uint64                    _sample_count = 0;
  int                       _time_start_count;
  time_t                    _time_start;

  FILE*                     _file_ptr = nullptr;

  std::string                    _datatype;
  std::string                    _vendor;
  std::string                    _proctype;
  std::string                    _waketype;

  std::mutex                _parser_mutex;
  std::mutex                _file_mutex;
  std::condition_variable        _cv;
  std::mutex                _cv_mutex;
};

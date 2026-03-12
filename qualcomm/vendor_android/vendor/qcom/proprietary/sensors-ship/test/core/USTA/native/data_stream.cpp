/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "data_stream.h"
#include <sensors_log.h>
#define MAX_ADB_LOGCAT_LENGTH 1000
using namespace ::com::quic::sensinghub::session::V1_0;

using namespace std;

data_stream::data_stream(suid sensor_uid,
    string datatype,
    string vendor,
    string filename,
    vector<string> &api,
    data_stream_event_cb push_event_cb,
    data_stream_error_cb push_error_cb) {
  _datatype.assign(datatype);
  _vendor.assign(vendor);
  _is_conn_cb_registered = false;
  _file_ptr = nullptr;
  _client_connect_id = -1;
  this->_sensor_uid.low = sensor_uid.low;
  this->_sensor_uid.high = sensor_uid.high;
  _proctype="";
  _waketype="";
  _push_event_cb = push_event_cb;
  _push_error_cb = push_error_cb;

  _stream_event_cb=[this](const uint8_t* msg , int msgLength, uint64_t time_stamp)
                { this->stream_event_cb(msg, msgLength, time_stamp); };

  _stream_resp_cb=[this](uint32_t resp_value, uint64_t client_connect_id)
          { this->stream_resp_cb(resp_value, client_connect_id); };

  _stream_error_cb =[this](ISession::error _error)
                { this->stream_error_cb(_error); };

  create_file(filename);
  _cb_thread = new worker();
  _parser = new MessageParser();
  if(nullptr == _cb_thread || nullptr == _parser)
    return;
  _parser->set_api_info(api);
  _time_start_count = 1;
}

data_stream::~data_stream() {
  if(nullptr != _file_ptr) {
    fflush(_file_ptr);
    fclose(_file_ptr);
    _file_ptr = nullptr;
  }
  if(nullptr != _cb_thread) {
    delete _cb_thread;
    _cb_thread = nullptr;
  }
  if(nullptr != _parser) {
    delete _parser;
    _parser = nullptr;
  }
}

int data_stream::send_request(
    ISession *session,
    sensors_diag_log* diag,
    send_req_msg_info& formated_request,
    client_req_msg_fileds &std_fields_data) {
  if(nullptr == session || nullptr == diag)
    return -1;

  _qmiSession = session;
  _diag = diag;

  if(false == _is_conn_cb_registered) {
    int ret = _qmiSession->setCallBacks(_sensor_uid,_stream_resp_cb,_stream_error_cb,_stream_event_cb);
    if(0 != ret)
      sns_loge("all callbacks are null, no need to register it");

    _is_conn_cb_registered = true;
  }

  usta_rc rc= USTA_RC_SUCCESS;
  string payload ;
  rc = get_std_payload(formated_request, payload);
  if(USTA_RC_SUCCESS != rc ) {
    return -1;
  }

  send_req_msg_info client_req_msg;
  client_req_msg.msg_name = "sns_client_request_msg";
  client_req_msg.msgid = formated_request.msgid;

  send_req_field_info suid_field;
  {
    suid_field.name = "suid";

    send_req_nested_msg_info suid_msg;
    {
      suid_msg.msg_name = "sns_std_suid";

      send_req_field_info suid_low_field;
      suid_low_field.name = "suid_low";
      suid_low_field.value.push_back(to_string(_sensor_uid.low));

      send_req_field_info suid_high_field;
      suid_high_field.name = "suid_high";
      suid_high_field.value.push_back(to_string(_sensor_uid.high));

      suid_msg.fields.push_back(suid_low_field);
      suid_msg.fields.push_back(suid_high_field);
    }
    suid_field.nested_msgs.push_back(suid_msg);
  }
  client_req_msg.fields.push_back(suid_field);

  send_req_field_info msg_id_field;
  {
    msg_id_field.name = "msg_id";
    msg_id_field.value.push_back(formated_request.msgid);
  }
  client_req_msg.fields.push_back(msg_id_field);

  send_req_field_info susp_config_field;
  {
    susp_config_field.name = "susp_config";

    send_req_nested_msg_info susp_config_msg;
    {
      susp_config_msg.msg_name = "suspend_config";
      send_req_field_info client_field;
      {
        client_field.name = "client_proc_type";
        if(!std_fields_data.client_type.empty()){
          client_field.value.push_back(std_fields_data.client_type);
          _proctype = std_fields_data.client_type;
        }
      }
      send_req_field_info wakeup_field;
      {
        wakeup_field.name = "delivery_type";
        if(!std_fields_data.wakeup_type.empty()){
          wakeup_field.value.push_back(std_fields_data.wakeup_type);
          _waketype = std_fields_data.wakeup_type;
        }
      }
      if(client_field.value.size() != 0)
        susp_config_msg.fields.push_back(client_field);
      if(wakeup_field.value.size() != 0)
        susp_config_msg.fields.push_back(wakeup_field);
    }
    if(susp_config_msg.fields.size() != 0)
      susp_config_field.nested_msgs.push_back(susp_config_msg);
  }
  if(susp_config_field.nested_msgs.size() != 0)
    client_req_msg.fields.push_back(susp_config_field);


  send_req_field_info request_field;
  {
    request_field.name = "request";

    send_req_nested_msg_info sns_std_request_msg;
    {
      sns_std_request_msg.msg_name = "sns_std_request";

      send_req_field_info batching_field;
      {
        batching_field.name = "batching";

        send_req_nested_msg_info batch_spec_msg;
        {
          batch_spec_msg.msg_name = "batch_spec";
          send_req_field_info batch_period_field;
          {
            batch_period_field.name = "batch_period";
            if(!std_fields_data.batch_period.empty())
            {
              batch_period_field.value.push_back(std_fields_data.batch_period);
            }
          }
          send_req_field_info flush_period_field;
          {
            flush_period_field.name = "flush_period";
            if(!std_fields_data.flush_period.empty())
            {
              flush_period_field.value.push_back(std_fields_data.flush_period);
            }

          }
          send_req_field_info flush_only_field;
          {
            flush_only_field.name = "flush_only";
            if(!std_fields_data.flush_only.empty())
            {
              flush_only_field.value.push_back(std_fields_data.flush_only);
            }
          }
          send_req_field_info max_batch_field;
          {
            max_batch_field.name = "max_batch";
            if(!std_fields_data.max_batch.empty())
            {
              max_batch_field.value.push_back(std_fields_data.max_batch);
            }
          }

          if(batch_period_field.value.size() != 0)
            batch_spec_msg.fields.push_back(batch_period_field);
          if(flush_period_field.value.size() != 0)
            batch_spec_msg.fields.push_back(flush_period_field);
          if(flush_only_field.value.size() != 0)
            batch_spec_msg.fields.push_back(flush_only_field);
          if(max_batch_field.value.size() != 0)
            batch_spec_msg.fields.push_back(max_batch_field);
        }
        if(batch_spec_msg.fields.size() != 0) {
          batching_field.nested_msgs.push_back(batch_spec_msg);
        }
      }
      if(batching_field.nested_msgs.size() != 0 )
        sns_std_request_msg.fields.push_back(batching_field);


      if(!std_fields_data.client_permissions.empty()) {
        send_req_field_info permissions_field;
        bool is_permissions_required = false;
        {
          permissions_field.name = "permissions";
          send_req_nested_msg_info permissions_msg;
          {
            permissions_msg.msg_name = "client_permissions";
            send_req_field_info technologies_field;
            {
              technologies_field.name = "technologies";
              for(int idx = 0; idx < std_fields_data.client_permissions.size(); ++idx)
              {
                technologies_field.value.push_back(std_fields_data.client_permissions.at(idx));
                is_permissions_required = true;
              }
            }
            if(!technologies_field.value.empty()) {
              permissions_msg.fields.push_back(technologies_field);
            }
          }
          if(!permissions_msg.fields.empty()) {
            permissions_field.nested_msgs.push_back(permissions_msg);
          }
        }
        if(!permissions_field.nested_msgs.empty()) {
          sns_std_request_msg.fields.push_back(permissions_field);
        }
      }

      send_req_field_info payload_field;
      {
        payload_field.name = "payload";
        if(!payload.empty()) {
          sns_logd("payload is not empty");
          payload_field.value.push_back(payload);
        }
      }

      if(payload_field.value.size() != 0)
        sns_std_request_msg.fields.push_back(payload_field);

      send_req_field_info is_passive_field;
      {
        is_passive_field.name = "is_passive";
        if(!std_fields_data.is_passive.empty())
        {
           is_passive_field.value.push_back(std_fields_data.is_passive);
        }
      }
      if(is_passive_field.value.size() != 0)
        sns_std_request_msg.fields.push_back(is_passive_field);

    }
    request_field.nested_msgs.push_back(sns_std_request_msg);
  }
  client_req_msg.fields.push_back(request_field);

  bool is_resampler_required = false;
  send_req_field_info resampler_field;
  {
    resampler_field.name = "resampler_config";
    send_req_nested_msg_info resampler_config_msg;
    {
      resampler_config_msg.msg_name = "sns_resampler_client_config";
      send_req_field_info resampler_rate_type;
      {
        resampler_rate_type.name = "rate_type";
        if(!std_fields_data.resampler_type.empty())
        {
          resampler_rate_type.value.push_back(std_fields_data.resampler_type);
          is_resampler_required = true;
        }
      }
      send_req_field_info resampler_filter;
      {
        resampler_filter.name = "filter";
        if(!std_fields_data.resampler_filter.empty())
        {
          resampler_filter.value.push_back(std_fields_data.resampler_filter);
          is_resampler_required = true;
        }
      }
      if(resampler_rate_type.value.size() != 0)
        resampler_config_msg.fields.push_back(resampler_rate_type);
      if(resampler_filter.value.size() != 0)
        resampler_config_msg.fields.push_back(resampler_filter);
    }
    if(resampler_config_msg.fields.size() != 0)
      resampler_field.nested_msgs.push_back(resampler_config_msg);
  }
  if(true == is_resampler_required) {
    client_req_msg.fields.push_back(resampler_field);
  }

  bool is_threshold_required = false;
  send_req_field_info threshold_field;
  {
    threshold_field.name = "threshold_config";
    send_req_nested_msg_info threshold_config_msg;
    {
      threshold_config_msg.msg_name = "sns_threshold_client_config";
      send_req_field_info threshold_type_field;
      {
        threshold_type_field.name = "threshold_type";
        if(!std_fields_data.threshold_type.empty())
        {
          threshold_type_field.value.push_back(std_fields_data.threshold_type);
          is_threshold_required = true;
        }
      }
      send_req_field_info threshold_value_field;
      {
        threshold_value_field.name = "threshold_val";
        if(!std_fields_data.threshold_value.empty())
        {
          for(int loop_index = 0; loop_index < std_fields_data.threshold_value.size(); loop_index++)
          {
            threshold_value_field.value.push_back(std_fields_data.threshold_value.at(loop_index));
            is_threshold_required = true;
          }
        }
      }
      if(threshold_type_field.value.size() != 0)
        threshold_config_msg.fields.push_back(threshold_type_field);
      if(threshold_value_field.value.size() != 0)
        threshold_config_msg.fields.push_back(threshold_value_field);
    }
    if(threshold_config_msg.fields.size() != 0)
      threshold_field.nested_msgs.push_back(threshold_config_msg);
  }
  if(true == is_threshold_required) {
    client_req_msg.fields.push_back(threshold_field);
  }
#ifdef SNS_CLIENT_TECH_ENABLED
  send_req_field_info client_tech;
  {
    client_tech.name = "client_tech";
    {
        client_tech.value.push_back("SNS_TECH_SENSORS");
    }
      client_req_msg.fields.push_back(client_tech);
  }
#endif
  string main_request_message("sns_client_request_msg");

  string main_request_encoded;
  if(USTA_RC_SUCCESS !=
      (rc=_parser->formulate_request_msg(client_req_msg,
          main_request_encoded)))
  {
    sns_loge("encoding main request failed");
    return -1;
  }
  std::unique_lock<std::mutex> parser_lk(_parser_mutex);
  rc = _parser->parse_request_message(client_req_msg.msg_name, main_request_encoded.c_str(),
      main_request_encoded.size());
  if(USTA_RC_SUCCESS != rc)
  {
    sns_loge("Error while Parse request message");
    return -1;
  }

  string req_json_string = _parser->get_request_json_string();
  sns_logd("%s" , req_json_string.c_str());

  std::unique_lock<std::mutex> file_lk(_file_mutex);
  if(sensors_log::get_level() == sensors_log::VERBOSE)
  {
    if(NULL != _file_ptr)
    {
      fprintf(_file_ptr, "%s" ,
          req_json_string.c_str());
      fflush(_file_ptr);
    }
  }
  file_lk.unlock();
  parser_lk.unlock();

  char logging_module_name[] = "USTA";
  if(nullptr != _diag)
    _diag->log_request_msg(
        main_request_encoded,_datatype, logging_module_name);

  std::unique_lock<std::mutex> lock(_cv_mutex);
  int ret = _qmiSession->sendRequest(_sensor_uid, main_request_encoded);
  if(0 != ret){
      sns_loge("Error in sending request");
      return -1;
  }
  sns_logi("%s", main_request_encoded.c_str());
  _cv.wait(lock);
  sns_logd("Send Request done , Wait is also done with clinet_connect_id %s", to_string(get_client_connect_id()).c_str());
  return get_client_connect_id();
}


void data_stream::stream_event_cb(const uint8_t *encoded_data_string,
    size_t encoded_string_size,
    uint64_t time_stamp) {
  string encoded_client_event_msg((char *)encoded_data_string,encoded_string_size);
  if(nullptr == _cb_thread || nullptr == _parser)
    return;
  _cb_thread->add_task([this, encoded_client_event_msg]{
    this->handle_event(encoded_client_event_msg);
  });
}

void data_stream::handle_event(string encoded_client_event_msg) {
  char logging_module_name[] = "USTA";
  if(nullptr != _diag)
    _diag->log_event_msg(encoded_client_event_msg,_datatype,logging_module_name);

  std::unique_lock<std::mutex> parser_lk(_parser_mutex);
  vector<payload_info> sensor_payload_info;
  _parser->parse_event_message((const char *)encoded_client_event_msg.c_str(),
      encoded_client_event_msg.length() , sensor_payload_info);

  string json_sensor_event = _parser->get_event_json_string();
  sns_logd("log_message_size %s", to_string(json_sensor_event.size()).c_str());
  if(json_sensor_event.size() < MAX_ADB_LOGCAT_LENGTH)
  {
    sns_logd("%s", json_sensor_event.c_str());
  }
  else
  {
    int maxLoopCount = json_sensor_event.size() / MAX_ADB_LOGCAT_LENGTH + 1;
    string remainingLogBuffer = json_sensor_event;
    int loopCount = 0;
    while(loopCount < maxLoopCount) {
      int startIndex = 0;
      int endIndex = (remainingLogBuffer.size() > MAX_ADB_LOGCAT_LENGTH )?MAX_ADB_LOGCAT_LENGTH:remainingLogBuffer.size();
      string subString = remainingLogBuffer.substr(startIndex, endIndex);
      sns_logd("%s", subString.c_str());
      loopCount = loopCount + 1 ;
      remainingLogBuffer = remainingLogBuffer.substr(endIndex );
    }
  }

  string currentSample = "";
  for(unsigned int i =0; i < sensor_payload_info.size() ; i++)
  {
    if(sensor_payload_info[i].name.empty())
      continue;
    currentSample += sensor_payload_info[i].name;
    currentSample += "::";
    for(unsigned int j=0; j < sensor_payload_info[i].value.size() ; j++)
    {
      currentSample += sensor_payload_info[i].value[j];
      if(j ==  sensor_payload_info[i].value.size() - 1)
        currentSample += ";";
      else
        currentSample += ",";
    }
    currentSample += "\n";
  }

  std::unique_lock<std::mutex> file_lk(_file_mutex);
  if(sensors_log::get_level() == sensors_log::VERBOSE)
  {
    if(NULL != _file_ptr)
    {
      fprintf(_file_ptr, "%s" ,
          json_sensor_event.c_str());
      fflush(_file_ptr);
    }
  }
  file_lk.unlock();
  parser_lk.unlock();
  _sample_count++;
  sns_logd("%s event message count %s", _datatype.c_str(), to_string(_sample_count).c_str());

  string adaptedS= json_sensor_event;
  adaptedS+=",\n \"Event Counter\" : "+to_string(_sample_count)+",\n";
  if(_time_start_count==1){
   time(&_time_start);
   _time_start_count=0;
  }
  time_t time_end;
  time(&time_end);
  double dif=difftime(time_end, _time_start);
  int time_elapsed=(int)dif;
  adaptedS+="\"Time Elapsed\" : "+to_string(time_elapsed)+"\n}";

  _push_event_cb(_qmiSession, adaptedS);
}

void data_stream::stream_resp_cb(uint32_t resp_value, uint64_t client_connect_id) {
  if(nullptr == _cb_thread)
    return;
  _cb_thread->add_task([this, resp_value, client_connect_id]{
    std::unique_lock<std::mutex> lock(_cv_mutex);
    this->handle_resp(resp_value, client_connect_id);
    set_client_connect_id(client_connect_id);
    _cv.notify_one();
  });
}

void data_stream::handle_resp(uint32_t resp_value, uint64_t client_connect_id) {
  char logging_module_name[] = "USTA";
  if(nullptr != _diag) {
    _diag->update_client_connect_id(client_connect_id);
    _diag->log_response_msg(
        resp_value,_datatype,logging_module_name);
  }

  string resp_msg_name = "sns_client_response_msg";
  string suid_name = "suid";
  string suid_low_name = "suid_low";
  string suid_high_name = "suid_high";
  string sns_std_error_name = "sns_std_error";
  string value_name = "value";

  stringstream ss_low;
  ss_low << std::hex << _sensor_uid.low;

  stringstream ss_high;
  ss_high << std::hex << _sensor_uid.high;

  string resp_msg = "";
  resp_msg += "\"" + resp_msg_name + "\"";
  resp_msg += " : {";
  resp_msg += "\n";
  resp_msg += " ";
  resp_msg += "\"" + suid_name + "\"";
  resp_msg += " : {";
  resp_msg += "\n";
  resp_msg += "  ";
  resp_msg += "\"" + suid_low_name + "\"";
  resp_msg += " : ";
  resp_msg += "\"" ;
  resp_msg += "0x" + ss_low.str() + "\"";
  resp_msg += ",";
  resp_msg += "\n";
  resp_msg += "  ";
  resp_msg += "\"" + suid_high_name + "\"";
  resp_msg += " : ";
  resp_msg += "\"" ;
  resp_msg += "0x" + ss_high.str() + "\"";
  resp_msg += ",";
  resp_msg += "\n";
  resp_msg += "},";
  resp_msg += "\n";
  resp_msg += "\""+ sns_std_error_name + "\"";
  resp_msg += " : {";
  resp_msg += "\n";
  resp_msg += " ";
  resp_msg += "\"" + value_name + "\"";
  resp_msg += " : ";
  resp_msg += "\"" + get_error_name(resp_value) + "\"";
  resp_msg += "\n";
  resp_msg += " }";
  resp_msg += "\n";
  resp_msg += "}";
  std::lock_guard<std::mutex> lk(_file_mutex);
  if(NULL != _file_ptr)
  {
    fprintf(_file_ptr, "%s" ,resp_msg.c_str());
    fflush(_file_ptr);
  }
  sns_logi("\n%s" , resp_msg.c_str());
}

void data_stream::handle_error(ISession::error error_code) {
    sns_loge("%s" , __func__);
    _push_error_cb(error_code);
}

void data_stream::stream_error_cb(ISession::error error_code) {
  if(nullptr == _cb_thread)
    return;
  _cb_thread->add_task([this, error_code]{
    this->handle_error(error_code);
  });
}

usta_rc data_stream::get_std_payload(send_req_msg_info& formated_request, string &std_payload) {
  string encoded_sns_request;
  usta_rc rc= USTA_RC_SUCCESS;
  if(USTA_RC_SUCCESS !=
      (rc=_parser->formulate_request_msg(formated_request,
          encoded_sns_request)))
  {
    if(rc != USTA_RC_DESC_FAILED)
    {
      /* USTA_RC_DESC_FAILED means, message name was not found
       * in the descriptor pool even though message id was present.
       * This is the case with on-change sensors.
       * Added this condition to support on-change sensors from USTA
       */
      return rc;
    }
  }
  std::lock_guard<std::mutex> lk(_parser_mutex);
  rc = _parser->parse_request_message(formated_request.msg_name, encoded_sns_request.c_str(),
      encoded_sns_request.size());
  if(USTA_RC_SUCCESS != rc)
  {
    if(encoded_sns_request.empty()) {
      sns_logd("encoded string is empty");
    }
    else{
      sns_logd("Encoded string is not empty");
    }
  }
  std_payload.assign(encoded_sns_request);
  return USTA_RC_SUCCESS;
}

string data_stream::get_error_name(uint32_t resp_value) {
  string enum_name = "";
  switch(resp_value)
  {
    case 0:
      enum_name += "SNS_STD_ERROR_NO_ERROR";
      break;
    case 1:
      enum_name += "SNS_STD_ERROR_FAILED";
      break;
    case 2:
      enum_name += "SNS_STD_ERROR_NOT_SUPPORTED";
      break;
    case 3:
      enum_name += "SNS_STD_ERROR_INVALID_TYPE";
      break;
    case 4:
      enum_name += "SNS_STD_ERROR_INVALID_STATE";
      break;
    case 5:
      enum_name += "SNS_STD_ERROR_INVALID_VALUE";
      break;
    case 6:
      enum_name += "SNS_STD_ERROR_NOT_AVAILABLE";
      break;
    case 7:
      enum_name +="SNS_STD_ERROR_POLICY";
      break;
    default:
      enum_name +="InValid Error Type";
      break;
  }
  return enum_name;
}

void data_stream::create_file(string filename) {
  string abs_path ;
  if(0 == filename.length()) {
    abs_path = LOG_FOLDER_PATH +
        _datatype + "_" + _vendor + ".txt";
  } else {
    abs_path = LOG_FOLDER_PATH + _datatype + "_" + _vendor + "_" + filename;
  }
  sns_logd("created file %s", abs_path.c_str());
  if(sensors_log::get_level() == sensors_log::VERBOSE)
  {
    _file_ptr = fopen(abs_path.c_str() , "w");
    if(nullptr == _file_ptr){
      sns_loge("logFile creation failed for %s", abs_path.c_str());
    }
  }
}

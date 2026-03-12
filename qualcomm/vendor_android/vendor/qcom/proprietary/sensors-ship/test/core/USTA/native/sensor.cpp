/*============================================================================
  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.

  @file sensor.cpp
  @brief
  sensor class implementation.
============================================================================*/

#include "sensor.h"
#include <sensors_log.h>

using namespace std;

extern DescriptorPool pool;
#define MAX_ADB_LOGCAT_LENGTH 1000
Sensor::Sensor(handle_attribute_event_func attrib_event_f,
    const uint64_t& suid_low,const uint64_t& suid_high,
    sensor_stream_event_cb push_event_cb,
    sensor_stream_error_cb push_error_cb)
{
  attrib_event_cb = attrib_event_f;
  _sensor_uid.low=suid_low;
  _sensor_uid.high=suid_high;

  _attrb_event_cb=[this](const uint8_t* msg , int msgLength, uint64_t time_stamp)
                { this->sensor_attrib_event_cb(msg, msgLength, time_stamp); };

  sensor_attribute_obj = NULL;

  if (NULL == (sensor_parser = new MessageParser()))
  {
    sns_loge(" message parser creation failed ");
  }
  _stream_event_cb=[this](ISession* session, string stream_event)
          { this->stream_event_cb(session,stream_event); };

  _stream_error_cb=[this](ISession::error stream_error)
            { this->stream_error_cb(stream_error); };

  _push_event_cb = push_event_cb;
  _push_error_cb = push_error_cb;

  _cb_thread = new worker();
}
Sensor::~Sensor()
{
  if (sensor_attribute_obj) delete sensor_attribute_obj;
  sensor_attribute_obj = NULL;

  if (sensor_parser) delete sensor_parser;
  sensor_parser=NULL;

  if(nullptr != _diag_attrb){
    delete _diag_attrb;
    _diag_attrb = nullptr;
  }

  if(nullptr != _cb_thread) {
    delete _cb_thread;
    _cb_thread = nullptr;
  }

  _data_stream_map_table.clear();
}

usta_rc Sensor::send_attrib_request(ISession* session_attrb, sensors_diag_log* diag_attrb)
{
  usta_rc rc= USTA_RC_SUCCESS;
  if(nullptr == session_attrb || nullptr == diag_attrb){
    sns_loge("invalid Input - session_attrb || diag_attrb");
    return USTA_RC_FAILED;
  }
  _session_attrb = session_attrb;
  if(nullptr != _session_attrb && false == _is_session_cb_registered) {
    int ret = _session_attrb->setCallBacks(_sensor_uid,_attrb_resp_cb,_attrb_error_cb,_attrb_event_cb);
    if(0 != ret)
      sns_loge("all callbacks are null, no need to register it");
    _is_session_cb_registered = true;
  }

  const EnumValueDescriptor *sns_attrib_req_evd =
      pool.FindEnumValueByName("SNS_STD_MSGID_SNS_STD_ATTR_REQ");

  if (NULL == sns_attrib_req_evd)
  {
    sns_loge(" SNS_STD_MSGID_SNS_STD_ATTR_REQ not found in descriptor pool");
    return USTA_RC_DESC_FAILED;
  }
  // formulating main message
  send_req_msg_info client_req_msg;
  client_req_msg.msg_name = "sns_client_request_msg";
  client_req_msg.msgid = (to_string(sns_attrib_req_evd->number()));

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
    msg_id_field.value.push_back(to_string(sns_attrib_req_evd->number()));
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
        client_field.value.push_back("SNS_STD_CLIENT_PROCESSOR_APSS");
      }
      send_req_field_info wakeup_field;
      {
        wakeup_field.name = "delivery_type";
        wakeup_field.value.push_back("SNS_CLIENT_DELIVERY_WAKEUP");
      }
      susp_config_msg.fields.push_back(client_field);
      susp_config_msg.fields.push_back(wakeup_field);
    }
    susp_config_field.nested_msgs.push_back(susp_config_msg);
  }
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
            batch_period_field.value.push_back("0");
          }
          send_req_field_info flush_period_field;
          {
            flush_period_field.name = "flush_period";
            flush_period_field.value.push_back("0");
          }
          send_req_field_info flush_only_field;
          {
            flush_only_field.name = "flush_only";
            flush_only_field.value.push_back(to_string(false));
          }
          send_req_field_info max_batch_field;
          {
            max_batch_field.name = "max_batch";
            max_batch_field.value.push_back(to_string(false));
          }
          batch_spec_msg.fields.push_back(batch_period_field);
          batch_spec_msg.fields.push_back(flush_period_field);
          batch_spec_msg.fields.push_back(flush_only_field);
          batch_spec_msg.fields.push_back(max_batch_field);
        }
        batching_field.nested_msgs.push_back(batch_spec_msg);
      }
      sns_std_request_msg.fields.push_back(batching_field);
    }
    request_field.nested_msgs.push_back(sns_std_request_msg);
  }
  client_req_msg.fields.push_back(request_field);

#ifdef SNS_CLIENT_TECH_ENABLED
  send_req_field_info client_tech;
  {
    client_tech.name = "client_tech";
    client_tech.value.push_back("SNS_TECH_SENSORS");
  }
  client_req_msg.fields.push_back(client_tech);
#endif

  //encoding the final request
  string sns_request_encoded;

  if(USTA_RC_SUCCESS !=
      (rc=sensor_parser->formulate_request_msg(client_req_msg,
          sns_request_encoded)))
  {
    sns_loge("Encoding main request for attribute failed");
    return rc;
  }

  rc = sensor_parser->parse_request_message(client_req_msg.msg_name,sns_request_encoded.c_str(),
      sns_request_encoded.size());
  if(USTA_RC_SUCCESS != rc)
  {
    sns_loge("Error while parse request message ");
    return rc;
  }

  sns_logd("sending attrb query on newer connection");
  if(nullptr != _session_attrb){
    int ret = _session_attrb->sendRequest(_sensor_uid, sns_request_encoded);
     if(0 != ret){
        sns_loge("Error in sending request");
        return USTA_RC_FAILED;
    }
  }

  return USTA_RC_SUCCESS;
}

void Sensor::sensor_attrib_event_cb(const uint8_t *encoded_data_string,
    size_t encoded_string_size,
    uint64_t time_stamp)
{
  string encoded_client_event_msg((char *)encoded_data_string,encoded_string_size);

  const Descriptor* message_descriptor =
      pool.FindMessageTypeByName("sns_client_event_msg");
  if(message_descriptor == NULL)
  {
    sns_loge("sensor_attrib_event_cb: message_descriptor is NULL");
    //message_descriptor is returned by google proto buffer.
    // Possible only if sns_client_event_msg is missing in proto file.
    return;
  }

  DynamicMessageFactory dmf;
  const Message* prototype = dmf.GetPrototype(message_descriptor);
  if(prototype == NULL)
  {
    sns_loge("sensor_attrib_event_cb: prototype is NULL");

    // It is common to all the sensors
    //prototype is returned by google proto buffer.
    // Which is highly impossible.
    return;
  }

  Message* message = prototype->New();
  if(message == NULL)
  {
    sns_loge("sensor_attrib_event_cb: message is NULL");

    // It is common to all the sensors
    //message is returned by google proto buffer.
    // Which is highly impossible.
   return;

  }

  message->ParseFromArray((void *)encoded_data_string,encoded_string_size);

  const Reflection* reflection = message->GetReflection();
  if(reflection == NULL)
  {
    sns_loge("sensor_attrib_event_cb: reflection is NULL");
    //reflection is returned by google proto buffer.
    // Which is highly impossible.
    return;

  }
  //decoding each element of sns_client_event_msg using their field descriptors
  /* first element is suid of type sns_std_suid */
  const FieldDescriptor* suid_field_desc =
      message_descriptor->FindFieldByName("suid");
  if(suid_field_desc == NULL)
  {
    sns_loge("sensor_attrib_event_cb: suid_field_desc is NULL");
    return; //It is w.r.t only this particular event. Shout not crash the app. Continue with next events
  }
  // since suid is of message type we need to get the sub_message
  Message* sub_message = reflection->MutableMessage(message, suid_field_desc);
  if(sub_message == NULL)
  {
    sns_loge("sensor_attrib_event_cb: sub_message is NULL");
    return; //It is w.r.t only this particular event. Shout not crash the app. Continue with next events
  }
  const Descriptor* suid_msg_desc = suid_field_desc->message_type();
  if(suid_msg_desc == NULL)
  {
    sns_loge("sensor_attrib_event_cb: suid_msg_desc is NULL");
    return;//It is w.r.t only this particular event. Shout not crash the app. Continue with next events
  }
  // way to get msg descriptor from field descriptor is field is a message type
  const FieldDescriptor *suid_low_field = suid_msg_desc->FindFieldByName("suid_low");
  const FieldDescriptor *suid_high_field = suid_msg_desc->FindFieldByName("suid_high");
  if(suid_low_field == NULL ||
      suid_high_field == NULL)
  {
    sns_loge("(suid_low_field || suid_high_field) == NULL");
    return;
  }

  /* second element is repeated event of type sns_client_event*/

  const FieldDescriptor* events_field_desc =
      message_descriptor->FindFieldByName("events");
  if(events_field_desc == NULL)
  {
    sns_loge("sensor_attrib_event_cb: events_field_desc is NULL");
    return;//It is w.r.t only this particular event. Shout not crash the app. Continue with next events
  }
  int num_events = reflection->FieldSize(*message, events_field_desc);

  if(num_events != 1)
  {
    return ;
  }

  // parse for printing in json format
  vector<payload_info> sensor_payload_info;
  sensor_parser->parse_event_message((const char *)encoded_data_string,
      encoded_string_size, sensor_payload_info);
  attrib_json_string =  sensor_parser->get_event_json_string();


  // assuming only one event, which is valid assumption for attribute
  sub_message =
      reflection->MutableRepeatedMessage(message, events_field_desc, 0);
  const Descriptor* events_msg_desc = events_field_desc->message_type();
  if(events_msg_desc == NULL)
  {
    sns_loge("sensor_attrib_event_cb: events_msg_desc is NULL");
    return;//It is w.r.t only this particular event. Shout not crash the app. Continue with next events
  }
  //way to get msg descriptor from field descriptor is field is a message type
  const FieldDescriptor *msg_id_field = events_msg_desc->FindFieldByName("msg_id");
  if(msg_id_field == NULL)
  {
    sns_loge("msg_id_field is NULL");
    return;
  }
  uint32 msg_id = sub_message->GetReflection()->GetUInt32(*sub_message,msg_id_field);
  const EnumValueDescriptor *sns_flush_event_evd =
      pool.FindEnumValueByName("SNS_STD_MSGID_SNS_STD_FLUSH_EVENT");
  if(sns_flush_event_evd == NULL)
  {
    sns_loge("sensor_attrib_event_cb: sns_flush_event_evd is NULL");
    return;//sns_flush_event_evd is NULL if SNS_STD_MSGID_SNS_STD_FLUSH_EVENT is not present in proto. In this case, skip this event.
  }
  uint32 flush_event_msgid = sns_flush_event_evd->number();
  if(msg_id == flush_event_msgid)
  {
    sns_logd("Avoiding Flush Event");
    return;//flush_event_msgid is NULL if SNS_STD_MSGID_SNS_STD_FLUSH_EVENT is not present in proto. In this case, skip this event.
  }

  const FieldDescriptor *payload_field = events_msg_desc->FindFieldByName("payload");
  if(payload_field == NULL)
  {
    sns_loge("payload_field is NULL");
    return;
  }
  string payload = sub_message->GetReflection()->GetString(*sub_message,payload_field);


  if (NULL == (sensor_attribute_obj = new sensor_attribute()))
  {
    sns_loge(" failed to created SensorAttributeObj");
    return; // Continue with next attribute event. Should not force crash the application
  }
  else
  {
    if(USTA_RC_SUCCESS != sensor_attribute_obj->store(payload))
      return; // Continue with next attribute event. Should not force crash the application

  }

  _datatype = sensor_attribute_obj->get_dataType();
  _vendor = sensor_attribute_obj->get_vendor();
  sensor_attribute_obj->get_max_sample_rate();

  char logging_module_name[] = "USTA";
  if(nullptr != _diag_attrb)
    _diag_attrb->log_event_msg(
        encoded_client_event_msg,_datatype,logging_module_name);

  if (attrib_event_cb) attrib_event_cb();

  if(NULL != message) {
    delete message;
    message = NULL;
  }

}

usta_rc Sensor::get_sensor_info(sensor_info& sensor_info_var)
{
  stringstream suidLow;
  suidLow << std::hex << _sensor_uid.low;

  stringstream suidHigh;
  suidHigh << std::hex << _sensor_uid.high;

  sensor_info_var.data_type = _datatype;
  sensor_info_var.vendor = _vendor;
  sensor_info_var.suid_high = suidHigh.str();
  sensor_info_var.suid_low = suidLow.str();

  return USTA_RC_SUCCESS;
}

usta_rc Sensor::get_request_list(vector<msg_info> &request_msgs)
{
  usta_rc rc=USTA_RC_SUCCESS;
  if (sensor_parser!=NULL)
  {
    if (sensor_attribute_obj->is_available(sns_std_sensor_attrid_api))
      _api = sensor_attribute_obj->get_API();
    else
    {
      sns_loge("API attribute not found");
      return USTA_RC_ATTRIB_FAILED;
    }

    if(USTA_RC_SUCCESS != (rc=sensor_parser->set_api_info(_api)))
    {
      sns_loge(" get request failed");
      return rc;
    }
    if(USTA_RC_SUCCESS!= (rc=sensor_parser->get_request_msg(request_msgs)))
    {
      sns_loge(" get request failed");
      return rc;
    }
    return rc;
  }

  return USTA_RC_FAILED;  // if sensor_parser is NULL

}

usta_rc Sensor::get_attributes(string& attribute_json_format)
{
  if (attrib_json_string.size() != 0)
    attribute_json_format.assign(attrib_json_string);
  else
  {
    sns_loge(" Attribute list is Empty");
  }

  return USTA_RC_SUCCESS;
}

void Sensor::get_payload_string(send_req_msg_info sensor_payload_field, string &encoded_data)
{
  usta_rc rc= USTA_RC_SUCCESS;
  string payload;
  if(USTA_RC_SUCCESS !=
      (rc=sensor_parser->formulate_request_msg(sensor_payload_field,
          payload)))
  {
    sns_loge("sensor_payload_field request failed");
  }
  encoded_data.assign(payload);
}

int Sensor::send_request(ISession *session,
    sensors_diag_log* diag,
    send_req_msg_info& std_sensor_req,
    client_req_msg_fileds &std_client_fields,
    string& log_file_name) {
  bool is_already_streaming = is_streaming(session);
  data_stream* stream_instance = nullptr;
  if(std_sensor_req.msgid.compare("10") == 0) {
     if(false == is_already_streaming){
       sns_loge("already in stop mode");
       return -1;
     } else {
       stream_instance = _data_stream_map_table.find(session)->second;
       int client_connect_id = stream_instance->send_request(session, diag, std_sensor_req, std_client_fields);
       _data_stream_map_table.erase(session);
       delete stream_instance;
       stream_instance = nullptr;
       return client_connect_id;
     }
  } else {
    if(true == is_already_streaming) {
      stream_instance = _data_stream_map_table.find(session)->second;
    } else {
      stream_instance = new data_stream(_sensor_uid,
          _datatype,
          _vendor,
          log_file_name,
          _api,
          _stream_event_cb,
          _stream_error_cb);
      if(nullptr == stream_instance)
        return -1;
      _data_stream_map_table.insert(pair<ISession* , data_stream*>(session, stream_instance));
    }
    return stream_instance->send_request(session, diag, std_sensor_req, std_client_fields);
  }
}

bool Sensor::is_streaming(ISession *session){
  auto it = _data_stream_map_table.find(session);
  if(it != _data_stream_map_table.end()) {
    sns_logi("is_streaming - True");
    return true;
  }
  else {
    sns_logi("is_streaming - False");
    return false;
  }
}

void Sensor::stream_event_cb(ISession* session, string sensor_event) {
  if(nullptr == _cb_thread)
    return;
  std::lock_guard<mutex> lk(_sync_mutex);
  _cb_thread->add_task([session, sensor_event,this]{
    if(this->_push_event_cb)
      this->_push_event_cb(session, sensor_event, (void *)this);
  });
}

void Sensor::stream_error_cb(ISession::error stream_error) {
  if(nullptr == _cb_thread)
    return;
  std::lock_guard<mutex> lk(_sync_mutex);
  _push_error_cb(stream_error);
}

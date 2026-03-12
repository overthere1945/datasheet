/*============================================================================
  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc

  @file sensor_cxt.cpp

  @brief
  SensorContext class implementation.
============================================================================*/
#include "sensor_cxt.h"
#include <algorithm>  //for using sort of vector
#include <stdlib.h>

#include "sensors_timeutil.h"
#include <sensors_log.h>

using namespace std;

#define MAX_GLINK_TRANSPORT_NAME 20

unique_ptr<sessionFactory> SensorCxt::_session_factory_instance = nullptr;

bool SensorCxt::waitForResponse(std::mutex& _cb_mutex,std::condition_variable& _cb_cond, bool& cond_var)
{
  bool ret = false;            /* the return value of this function */


  /* special case where callback is issued before the main function
       can wait on cond */
  unique_lock<mutex> lk(_cb_mutex);
  if (cond_var == true)
  {
    ret = true;
  }
  else
  {
    while (cond_var != true)
    {
      _cb_cond.wait(lk);
    }
    ret = ( cond_var ) ? true : false;
  }
  cond_var = false;
  return ret;
}

SensorCxt* SensorCxt::getInstance()
{
  SensorCxt* self = new SensorCxt();
  return self;
}

/*============================================================================
  SensorCxt Constructor
============================================================================*/
SensorCxt::SensorCxt()
{
  sensors_log::set_tag("USTA_NATIVE");
  sensors_log::set_level(sensors_log::INFO);
  sns_logi(" USTA Native init ");
  //intializing condition variable and mutex
  attributes_info.max_sampling_rate = 0.0;
  attributes_info.max_report_rate = 0.0;
  num_sensors = 0;
  num_attr_received = 0;
  std::vector<ISession*> _attrbSession;

  is_resp_arrived=false;
  suid_instance = nullptr;
  _diag_attrb = nullptr;

  if(nullptr == _session_factory_instance) {
     _session_factory_instance = make_unique<sessionFactory>();
     if(nullptr == _session_factory_instance){
       printf("failed to create factory instance");
       return;
     }
   }

  _hub_ids = _session_factory_instance->getSensingHubIds();
  vector<int> supported_hub_ids;

  _sensor_event_cb = [this](ISession* conn, string sensor_event, void* sensor_instance)
                  { this->process_sensor_event(conn, sensor_event, sensor_instance); };

  _sensor_error_cb = [this](ISession::error sensor_error)
                      { this->process_sensor_error(sensor_error); };

  //initializing sensor event callback
  suid_event_cb = [this](uint64_t& suid_low,uint64_t& suid_high, bool flag)
    { this->handle_suid_event(suid_low,suid_high,flag);};

  attribute_event_cb =[this]()
    { this->handle_attribute_event();};

  // building descriptor pool
  if (USTA_RC_SUCCESS != build_descriptor_pool())
  {
    sns_loge("Descriptor pool generation failed.. exiting the application ");
    assert(false);
  }

  // first SUID class object is created and request is sent for listing
  //all the sensors
  for (int loop_hub_idx = 0; loop_hub_idx < _hub_ids.size(); ++loop_hub_idx) {
    _current_hub_id = _hub_ids[loop_hub_idx];
    ISession* _suidSession = _session_factory_instance->getSession(_current_hub_id);
    if(nullptr == _suidSession){
      sns_loge("failed to create session for suid discovery");
      continue;
    }
    int ret = _suidSession->open();
    if(-1 == ret)   continue;

    if (NULL == (suid_instance = new SuidSensor(suid_event_cb, _suidSession)))
    {
      sns_loge("suid instance creation failed for conn_type ");
      continue;
    }

    string list_req_special_char = "";
    if (USTA_RC_SUCCESS !=(suid_instance->send_request(list_req_special_char)))
    {
      sns_loge("\n SUID listing request failed ");
      continue;
    }

    if (this->waitForResponse(cb_mutex,cb_cond,is_resp_arrived) == false)
    {
      sns_loge("\nError  ");
    }
    else
    {
      _hub_id_idx_map_table[_hub_ids[loop_hub_idx]] = loop_hub_idx;
      supported_hub_ids.push_back(_hub_ids[loop_hub_idx]);
      sns_logd("\nSensor list Response Received");
    }

    /* Can be stored only once as proto file will be same across sensing-hubs */
    if(_client_processor.size() == 0)
      _client_processor = suid_instance->get_client_processor_list();
    if(_wakeup_delivery_type.size() == 0)
      _wakeup_delivery_type = suid_instance->get_wakeup_delivery_list();
    if(_resampler_type.size() == 0)
      _resampler_type = suid_instance->get_resampler_type_list();
    if(_threshold_type.size() == 0)
      _threshold_type = suid_instance->get_threshold_type_list();

    // removing suid_instance
    sns_logd("removing suid instance for hub id = %d", _hub_ids[loop_hub_idx]);
    delete suid_instance;
    suid_instance = NULL;

    sns_logd("closing suid session for hub id = %d", _hub_ids[loop_hub_idx]);
    if(nullptr != _suidSession)
    {
      _suidSession->close();
      delete _suidSession;
      _suidSession = nullptr;
    }

    _current_hub_id = -1;
  }

  _attrbSession.resize(supported_hub_ids.size());

  for(int loop_idx = 0; loop_idx < supported_hub_ids.size(); loop_idx++) {
    /*Create a connection for Attribute Query*/
    _attrbSession[loop_idx] = create_qsh_interface(_hub_ids[loop_idx]);
    if(nullptr == _attrbSession[loop_idx]) {
      sns_loge("error while creating qsh interface instance for attrb query for hub_id = %d", _hub_ids[loop_idx]);
    }
  }

  // sending attrib request for each sensor
  int snsr_hndl = 0;
  for(auto sensorHandle = _sensors_hub_id_map_table.begin();
    sensorHandle != _sensors_hub_id_map_table.end(); sensorHandle++)
    {
      if(0 == _hub_id_idx_map_table.count(sensorHandle->second)) {
        sns_loge("Invalid hub_id = %d", sensorHandle->second);
        continue;
      }
      auto hub_id_idx = _hub_id_idx_map_table.at(sensorHandle->second);
      if(nullptr == _attrbSession[hub_id_idx]) {
        continue;
      }
      _diag_attrb = get_diag_ptr(_attrbSession[hub_id_idx]);
      if(nullptr == _diag_attrb) {
        sns_loge("error while getting _diag_attrb for hub_id = %d", sensorHandle->second);
        continue;
      }
      is_resp_arrived=false;
      sns_logi("Sending Attribute request for handle %d", snsr_hndl);
      (sensorHandle->first)->send_attrib_request(_attrbSession[hub_id_idx], _diag_attrb);
      // wait for respective event to come
      if (false == this->waitForResponse(cb_mutex,cb_cond,is_resp_arrived))
      {
        sns_loge("\nError  ");
      }
      else
      {
        sns_logi("Attribute  Response Received for handle %d", snsr_hndl);
      }
      snsr_hndl++;
    }

  for(int loop_idx = 0; loop_idx < _hub_ids.size(); loop_idx++)
  {
    sns_logi("Deleting attribute session for hub id = %d", _hub_ids[loop_idx]);
    if(nullptr != _attrbSession[loop_idx]) {
      delete_qsh_interface_info(_attrbSession[loop_idx]);
    }
  }

  _cb_thread = new worker();

  unsigned int sensor_position=0;
  vector<int> sensorsToBeRemoved;
  list<pair<Sensor*, int>>::iterator sensorHandle;
  vector<msg_info> request_msgs;
  for(sensorHandle = _sensors_hub_id_map_table.begin(); sensorHandle != _sensors_hub_id_map_table.end(); sensorHandle++,sensor_position++)
  {
    if(USTA_RC_SUCCESS!=(sensorHandle->first)->get_request_list(request_msgs))
    {
      sensorsToBeRemoved.push_back(sensor_position);
    }
  }
  remove_sensors(sensorsToBeRemoved);

  sns_logd("Sensor Context instantiated");
}

SensorCxt::~SensorCxt()
{
  sns_logd("Destroy Sensor Context");
  if (suid_instance)  delete suid_instance;
  suid_instance =NULL;
  if(nullptr != _cb_thread) {
    delete _cb_thread;
    _cb_thread = nullptr;
  }
  /* Deleting sensor memory */
  for(auto sensorHandle = _sensors_hub_id_map_table.begin();
      sensorHandle != _sensors_hub_id_map_table.end(); )
  {
    delete sensorHandle->first;
    sensorHandle->first = NULL;
    sensorHandle = _sensors_hub_id_map_table.erase(sensorHandle);
  }
  for(int loop_idx = 0 ; loop_idx < _qsh_interface_info_array.size() ; loop_idx++) {
    delete_qsh_interface_info(_qsh_interface_info_array.at(loop_idx).interface);
  }
  _qsh_interface_info_array.clear();
  _interface_map_table.clear();
  _diag_map_table.clear();
  _callback_map_table.clear();
  _client_connect_id_streaming_table.clear();

  sns_logd("Sensor Context is destroyed");
}

void SensorCxt::update_logging_flag(bool disable_flag)
{
  if(true == disable_flag)
  {
    sensors_log::set_level(sensors_log::INFO);
  }
  else
  {
    sensors_log::set_level(sensors_log::VERBOSE);
  }
}

usta_rc SensorCxt::get_request_list(unsigned int handle,vector<msg_info> &request_msgs)
{
  unsigned int i=0;
  list<pair<Sensor *, int>>::iterator sensorHandle;
  for(sensorHandle = _sensors_hub_id_map_table.begin(); sensorHandle != _sensors_hub_id_map_table.end();
      sensorHandle++,i++ )
  {
    if (handle==i) break;
  }
  if(i>=_sensors_hub_id_map_table.size())
  {
    sns_loge("Error handle %d exceding the sensor list size %u get_request_list failed", handle, (unsigned int)_sensors_hub_id_map_table.size());
    return USTA_RC_FAILED;
  }
  return (sensorHandle->first)->get_request_list(request_msgs);
}

usta_rc SensorCxt::get_attributes(unsigned int handle,string& attribute_list)
{
  unsigned int i=0;
  list<pair<Sensor *, int>>::iterator sensorHandle;
  for( sensorHandle = _sensors_hub_id_map_table.begin(); sensorHandle != _sensors_hub_id_map_table.end();
      sensorHandle++,i++ )
  {
    if (handle==i) break;
  }
  if(i>=_sensors_hub_id_map_table.size())
  {
    sns_loge("Error handle %d exceding the sensor list size %u get_request_list failed", handle, (unsigned int)_sensors_hub_id_map_table.size());
    return USTA_RC_FAILED;
  }

  return sensorHandle->first->get_attributes(attribute_list);
}

usta_rc SensorCxt::get_sensor_list(vector<sensor_info>& sensor_list)
{

  int i=0;
  for(auto sensorHandle = _sensors_hub_id_map_table.begin();
      sensorHandle != _sensors_hub_id_map_table.end(); sensorHandle++,i++)
  {
    sensor_info sensor_info_var;
    if(USTA_RC_SUCCESS!=sensorHandle->first->get_sensor_info(sensor_info_var))
    {
      sns_loge("Error handle %d not part of sensor list", i);
    }
    else
    {
      sensor_list.push_back(sensor_info_var);
    }
  }

  return USTA_RC_SUCCESS;

}

usta_rc SensorCxt::remove_sensors(vector<int>& remove_sensor_list)
{
  // sorting the handles in acending order
  sort(remove_sensor_list.begin(), remove_sensor_list.end());
  list<pair<Sensor*, int>>::iterator sensorHandle;
  for(int loop_idx = remove_sensor_list.size()-1; loop_idx >= 0; loop_idx--)
  {
    sensorHandle = next(_sensors_hub_id_map_table.begin(), remove_sensor_list[loop_idx]);
    delete sensorHandle->first;
    _sensors_hub_id_map_table.erase(sensorHandle);
  }

  return USTA_RC_SUCCESS;


}

bool SensorCxt::is_sensor_streaming(unsigned int sensor_handle) {
  auto it = _sensor_handle_map_table.find(sensor_handle);
  if(it == _sensor_handle_map_table.end())
    return false;
  else
    return true;
}

void SensorCxt::handle_suid_event(uint64_t& suid_low,uint64_t& suid_high,
    bool is_last_suid)
{
  // creating sensor to query attributes in constructor itself
  Sensor* sensorHandle = new Sensor(attribute_event_cb,
      suid_low,
      suid_high,
      _sensor_event_cb,
      _sensor_error_cb);
  _sensors_hub_id_map_table.push_back(make_pair(sensorHandle, _current_hub_id));

  num_sensors++;

  if (is_last_suid)
  {
    lock_guard<mutex> lk(cb_mutex);
    is_resp_arrived=true;
    cb_cond.notify_one();
  }
}

void SensorCxt::handle_attribute_event()
{
  lock_guard<mutex> lk(cb_mutex);
  is_resp_arrived=true;
  cb_cond.notify_one();

}

void SensorCxt::enableStreamingStatus(unsigned int handle)
{
  unsigned int i=0;
  list<pair<Sensor *, int>>::iterator sensorHandle;
  for(sensorHandle = _sensors_hub_id_map_table.begin(); sensorHandle != _sensors_hub_id_map_table.end();
      sensorHandle++,i++ )
  {
    if (handle==i) break;
  }
  if(i>=_sensors_hub_id_map_table.size())
  {
      sns_loge("Error handle %d exceding the sensor list size %u get_request_list failed", handle, (unsigned int)_sensors_hub_id_map_table.size());
    return;
  }
//  (*sensorHandle)->enableStreamingStatus();
}

void SensorCxt::disableStreamingStatus(unsigned int handle)
{
  unsigned int i=0;
  list<pair<Sensor *, int>>::iterator sensorHandle;
  for(sensorHandle = _sensors_hub_id_map_table.begin(); sensorHandle != _sensors_hub_id_map_table.end();
      sensorHandle++,i++ )
  {
    if (handle==i) break;
  }
  if(i>=_sensors_hub_id_map_table.size())
  {
    sns_loge("Error handle %d exceding the sensor list size %u get_request_list failed", handle, (unsigned int)_sensors_hub_id_map_table.size());
    return;
  }
//  (*sensorHandle)->disableStreamingStatus();
}

void SensorCxt::update_display_samples_cb(display_samples_cb ptr_reg_callback)
{
  ptr_display_samples_cb = ptr_reg_callback;
}

direct_channel_type SensorCxt::get_channel_type(sensor_channel_type in)
{
  switch(in) {
  case STRUCTURED_MUX_CHANNEL:
    return DIRECT_CHANNEL_TYPE_STRUCTURED_MUX_CHANNEL;
  case GENERIC_CHANNEL:
    return DIRECT_CHANNEL_TYPE_GENERIC_CHANNEL;
  default:
    return DIRECT_CHANNEL_TYPE_STRUCTURED_MUX_CHANNEL;
  }
}

sns_std_client_processor SensorCxt::get_client_type(sensor_client_type in)
{
  switch(in) {
  case SSC:
    return SNS_STD_CLIENT_PROCESSOR_SSC;
  case APSS:
    return SNS_STD_CLIENT_PROCESSOR_APSS;
  case ADSP:
    return SNS_STD_CLIENT_PROCESSOR_ADSP;
  case MDSP:
    return SNS_STD_CLIENT_PROCESSOR_MDSP;
  case CDSP:
    return SNS_STD_CLIENT_PROCESSOR_CDSP;
  default:
    return SNS_STD_CLIENT_PROCESSOR_APSS;
  }
}

usta_rc SensorCxt::direct_channel_open(create_channel_info create_info, unsigned long &channel_handle)
{
  sns_logi("direct_channel_open Start ");
  sensor_direct_channel *new_channel = new sensor_direct_channel(
        create_info.shared_memory_ptr,
        create_info.shared_memory_size,
        get_channel_type(create_info.channel_type_value),
        get_client_type(create_info.client_type_value)
        );
  if(NULL == new_channel)
  {
    sns_loge("Not able to establish the channel - NULL returned ");
    return USTA_RC_FAILED;
  }
  sns_logi("sensor_direct_channel created is %px " , new_channel);
  direct_channel_instance_array.push_back(new_channel);
  channel_handle = sensors_timeutil::get_instance().qtimer_get_ticks();
  sns_logi("channel_handle %lu", channel_handle);
  direct_channel_handle_array.push_back(channel_handle);
  sns_logi("direct_channel_open End ");
  return USTA_RC_SUCCESS;
}


usta_rc SensorCxt::direct_channel_close(unsigned long channel_handle)
{
  sns_logi("direct_channel_close Start with channel_handle %lu", channel_handle);
  sensor_direct_channel *direct_channel_instance = get_dc_instance(channel_handle);
  if(direct_channel_instance == NULL)
  {
    sns_loge("direct_channel_instance is NULL");
    return USTA_RC_FAILED;
  }

  delete direct_channel_instance;
  direct_channel_instance = NULL;

  sns_logi("direct_channel_close End ");
  return USTA_RC_SUCCESS;
}


usta_rc SensorCxt::direct_channel_update_offset_ts(unsigned long channel_handle, int64_t offset)
{
  sns_logi("direct_channel_update_offset_ts Start for channel handle %lu", channel_handle);

  sensor_direct_channel *direct_channel_instance = get_dc_instance(channel_handle);
  if(direct_channel_instance == NULL)
  {
    sns_loge("direct_channel_instance is NULL");
    return USTA_RC_FAILED;
  }

  int ret = direct_channel_instance->update_offset(offset);
  if(ret == 0) {
    sns_logi("direct_channel_update_offset_ts End ");
    return USTA_RC_SUCCESS;
  } else {
    sns_logi("direct_channel_update_offset_ts update_offset failed ");
    return USTA_RC_FAILED;
  }
   return USTA_RC_SUCCESS;
}
sensor_direct_channel* SensorCxt::get_dc_instance(unsigned long channel_handle)
{
  sns_logi("get_direct_channel_instance_from_channel_handle  %lu", channel_handle);
  unsigned int current_position = 0;
  for( ; current_position < direct_channel_handle_array.size(); current_position++)
  {
    sns_logi("current %lu Required %lu", direct_channel_handle_array.at(current_position), channel_handle);
    if(direct_channel_handle_array.at(current_position) == channel_handle) {
      sns_logi("found channel at position   %u", current_position);
      break;
    }
  }
  if(current_position == direct_channel_handle_array.size()) {
    sns_loge("Channel NOT FOUND ");
    return NULL;
  }

  unsigned int i = 0;
  list<sensor_direct_channel *>::iterator direct_channel_instance;
  for(direct_channel_instance = direct_channel_instance_array.begin(); direct_channel_instance != direct_channel_instance_array.end();
      direct_channel_instance++,i++ )
  {
    if (current_position==i) break;
  }
  if(i>=direct_channel_instance_array.size())
  {
    sns_loge("Error handle %u exceding the Channel list size %zu direct_channel_send_request failed", current_position, direct_channel_instance_array.size());
    return NULL;
  }
  return *direct_channel_instance;

}

usta_rc SensorCxt::direct_channel_send_request(unsigned long channel_handle, direct_channel_std_req_info std_req_info, send_req_msg_info sensor_payload_field)
{
  sns_logi("direct_channel_send_request Start with channel_handle %lu",channel_handle);

  sensor_direct_channel *direct_channel_instance = get_dc_instance(channel_handle);
  if(direct_channel_instance == NULL)
  {
    sns_loge("direct_channel_instance is NULL");
    return USTA_RC_FAILED;
  }
  /*Formulate as per expectations */
  direct_channel_stream_info stream_info;
  memset(&stream_info, 0, sizeof(direct_channel_stream_info));
  sns_logi("direct_channel_send_request current sensorHandle %u", std_req_info.sensor_handle);
  get_suid_info(std_req_info.sensor_handle , &stream_info);

  sns_logi("direct_channel_send_request get_suid_info Done with channel_handle %lu", channel_handle);

  if(!std_req_info.is_calibrated.empty()) {
    stream_info.has_calibrated = true;
    sns_logi("direct_channel_send_request has_calibrated True");
    if(std_req_info.is_calibrated.compare("true") == 0) {
      stream_info.is_calibrated = true;
      sns_logi("direct_channel_send_request is_calibrated True");
    }
    if(std_req_info.is_calibrated.compare("false") == 0) {
      stream_info.is_calibrated = false;
      sns_logi("direct_channel_send_request is_calibrated False");
    }
  }
  else
  {
    sns_logi("direct_channel_send_request has_calibrated false");
    stream_info.has_calibrated = false;
  }

  sns_logi("direct_channel_send_request is_calibrated Done with channel_handle %lu", channel_handle);

  if(!std_req_info.is_fixed_rate.empty()) {
    stream_info.has_fixed_rate = true;
    sns_logi("direct_channel_send_request has_fixed_rate True");
    if(std_req_info.is_fixed_rate.compare("true") == 0) {
      stream_info.is_resampled = true;
      sns_logi("direct_channel_send_request is_resampled True");
    }
    if(std_req_info.is_fixed_rate.compare("false") == 0) {
      stream_info.is_resampled = false;
      sns_logi("direct_channel_send_request is_resampled false");
    }
  }
  else
  {
    sns_logi("direct_channel_send_request has_fixed_rate True");
    stream_info.has_fixed_rate = false;
  }

  sns_logi("direct_channel_send_request is_fixed_rate Done with channel_handle %lu", channel_handle);
  sensor_direct_channel_srd_req_fileds std_req_fields_info;
  /*BATCH Period */
  if(!std_req_info.batch_period.empty())
  {
    std_req_fields_info.has_batch_period = true;
    std_req_fields_info.batch_period = stoi(std_req_info.batch_period);
  }
  else
  {
    std_req_fields_info.has_batch_period = false;
  }

  sns_logi("direct_channel_send_request batch_period Done with channel_handle %lu and batch_period %lu", channel_handle, std_req_fields_info.batch_period);

  direct_channel_mux_ch_attrb attributes_info;
  attributes_info.sensor_handle = std_req_info.sensor_handle;
  attributes_info.sensor_type = get_sensor_type(std_req_info.sensor_handle , stream_info.is_calibrated);

  bool has_attributes_info = true;

  sns_logi("direct_channel_send_request  attributes_info Done with channel_handle %lu", channel_handle);

  string sensor_req_encoded_payload;
  get_payload_string(std_req_info.sensor_handle , sensor_payload_field, sensor_req_encoded_payload);


  sns_logi("direct_channel_send_request  Before send_request  with channel_handle %lu", channel_handle);

  int ret = direct_channel_instance->send_request(
      stoi(sensor_payload_field.msgid),
      stream_info,
      std_req_fields_info,
      attributes_info,
      has_attributes_info,
      sensor_req_encoded_payload
  );

  if(0 != ret) {
    sns_loge("direct_channel_send_request - error in sending offest to DSP" );
    return USTA_RC_FAILED;
  }

  sns_logi("direct_channel_send_request End ");
  return USTA_RC_SUCCESS;
}

int SensorCxt::get_sensor_type(unsigned int handle, bool is_calibrated)
{

  unsigned int i=0;
  list<pair<Sensor *, int>>::iterator sensorHandle;
  for(sensorHandle = _sensors_hub_id_map_table.begin(); sensorHandle != _sensors_hub_id_map_table.end();
      sensorHandle++,i++ )
  {
    if (handle==i) break;
  }
  if(i>=_sensors_hub_id_map_table.size())
  {
    sns_loge("Error handle %u exceding the sensor list size %u get_sensor_type failed", handle, (unsigned int)_sensors_hub_id_map_table.size());
    return 0;
  }
  sensor_info sensor_info_var;
  sensorHandle->first->get_sensor_info(sensor_info_var);

  if(sensor_info_var.data_type.compare("accel") == 0) {
      if(is_calibrated)
        return 1;
      else
        return 35;
  } else if (sensor_info_var.data_type.compare("gyro") == 0) {
    if(is_calibrated)
      return 4;
    else
      return 16;
  } else if (sensor_info_var.data_type.compare("mag") == 0) {
    if(is_calibrated)
      return 2;
    else
      return 14;
  }
  return 0;

}

void SensorCxt::get_payload_string(unsigned int handle , send_req_msg_info sensor_payload_field, string &encoded_data)
{
  unsigned int i=0;
  list<pair<Sensor *,int>>::iterator sensorHandle;
  for(sensorHandle = _sensors_hub_id_map_table.begin(); sensorHandle != _sensors_hub_id_map_table.end();
      sensorHandle++,i++ )
  {
    if (handle==i) break;
  }
  if(i>=_sensors_hub_id_map_table.size())
  {
    sns_loge("Error handle %u exceding the sensor list size %u get_payload_string failed", handle, (unsigned int)_sensors_hub_id_map_table.size());
  }
  string payload;
  sensorHandle->first->get_payload_string(sensor_payload_field, payload);
  encoded_data.assign(payload);
}

usta_rc SensorCxt::direct_channel_delete_request(unsigned long channel_handle, direct_channel_delete_req_info delete_req_info)
{
  sns_logi("direct_channel_stop_request Start ");

  sensor_direct_channel *direct_channel_instance = get_dc_instance(channel_handle);
  if(direct_channel_instance == NULL)
  {
    sns_loge("direct_channel_instance is NULL");
    return USTA_RC_FAILED;
  }


  direct_channel_stream_info stream_info;

  get_suid_info(delete_req_info.sensor_handle , &stream_info);

  if(!delete_req_info.is_calibrated.empty()) {
    stream_info.has_calibrated = true;
    if(delete_req_info.is_calibrated.compare("true") == 0)
      stream_info.is_calibrated = true;
    else if(delete_req_info.is_calibrated.compare("false") == 0)
      stream_info.is_calibrated = false;
  }
  else
  {
    stream_info.has_calibrated = false;
  }

  if(!delete_req_info.is_fixed_rate.empty()) {
    stream_info.has_fixed_rate = true;
    if(delete_req_info.is_fixed_rate.compare("true") == 0)
      stream_info.is_resampled = true;
    if(delete_req_info.is_fixed_rate.compare("false") == 0)
      stream_info.is_resampled = false;
  }
  else
  {
    stream_info.has_fixed_rate = false;
  }

  int ret = direct_channel_instance->stop_request(stream_info);
  if(0 != ret) {
    sns_loge("direct_channel_update_offset_ts - error in sending offest to DSP" );
    return USTA_RC_FAILED;
  }

  sns_logi("direct_channel_stop_request End ");
  return USTA_RC_SUCCESS;
}

void SensorCxt::get_suid_info(unsigned int handle , direct_channel_stream_info *stream_info_out)
{
  unsigned int i=0;
  list<pair<Sensor *, int>>::iterator sensorHandle;
  for(sensorHandle = _sensors_hub_id_map_table.begin(); sensorHandle != _sensors_hub_id_map_table.end();
      sensorHandle++,i++ )
  {
    if (handle==i) break;
  }
  if(i>=_sensors_hub_id_map_table.size())
  {
    sns_loge("Error handle %u exceding the sensor list size %u get_request_list failed", handle, (unsigned int)_sensors_hub_id_map_table.size());
    return;
  }
  sensor_info sensor_info_var;
  sensorHandle->first->get_sensor_info(sensor_info_var);

  unsigned long suid_low = 0;
  istringstream(sensor_info_var.suid_low) >> hex >> suid_low;

  unsigned long suid_high = 0;
  istringstream(sensor_info_var.suid_high) >> hex >> suid_high;

  stream_info_out->suid_high = suid_high;
  stream_info_out->suid_low = suid_low;

}

int SensorCxt::send_request(client_request client_req,
    unsigned int sensor_handle,
    send_req_msg_info& request_msg,
    client_req_msg_fileds &std_fields_data,
    string& log_file_name)
{
  int client_connect_id = -1;
  auto current_sensor = get_sensor_data(sensor_handle);
  if(nullptr == current_sensor.first) {
    return -1;
  }
  if(CREATE_CLIENT == client_req.req_type) {
    sns_logi("send_request for client_connect_id is received - Start - CREATE_CLIENT");
    unique_lock<mutex> map_lock(_map_mutex);

    ISession* interface_instance = create_qsh_interface(current_sensor.second);
    if(nullptr == interface_instance) {
      return -1;
    }
    sensors_diag_log* diag = get_diag_ptr(interface_instance);
    if(nullptr == diag) {
      return -1;
    }
    map_lock.unlock();
    client_connect_id = send_request(interface_instance, diag, current_sensor.first, request_msg, std_fields_data, log_file_name);
    map_lock.lock();
    _interface_map_table.insert(pair<int,ISession*>(client_connect_id, interface_instance));
    _diag_map_table.insert(pair<int,sensors_diag_log*>(client_connect_id, diag));
    vector<unsigned int> sensors_streaming_list;
    sensors_streaming_list.push_back(sensor_handle);
    _client_connect_id_streaming_table.insert(pair<int,vector<unsigned int>>(client_connect_id,sensors_streaming_list));
    register_cb current_cb;
    current_cb.event_cb = client_req.cb_ptr.event_cb;
    _callback_map_table.insert(pair<int,register_cb>(client_connect_id, current_cb));
  } else if(USE_CLIENT_CONNECT_ID == client_req.req_type) {
    sns_logi("send_request for client_connect_id is received - Start - with client_id %u", client_req.client_connect_id);
    ISession *interface_instance = nullptr;
    unique_lock<mutex> map_lock(_map_mutex);
    if( _interface_map_table.end() != _interface_map_table.find(client_req.client_connect_id))
      interface_instance = _interface_map_table.find(client_req.client_connect_id)->second;
    else{
      sns_loge(" wrong input from client, client_connect_id:%u not found in interface map table", client_req.client_connect_id);
      return -1;
    }
    sensors_diag_log* diag = nullptr;
    if(_diag_map_table.end() != _diag_map_table.find(client_req.client_connect_id))
      diag = _diag_map_table.find(client_req.client_connect_id)->second;
    else{
      sns_loge(" wrong input from client, client_connect_id:%u not found in interface map table", client_req.client_connect_id);
      return -1;
    }
    map_lock.unlock();
    client_connect_id = send_request(interface_instance, diag, current_sensor.first, request_msg, std_fields_data, log_file_name);
//    sns_logi("USE_CLIENT_CONNECT_ID send_request complted with client_connect_id %u for sensor_handle %u with msg ID %s", client_connect_id, sensor_handle, request_msg.msgid.c_str());
    if(client_connect_id < 0)
      return client_connect_id;
    if(!request_msg.msgid.compare("10")) {
      sns_logi("USE_CLIENT_CONNECT_ID with msg ID 10");
      map_lock.lock();

      auto& sensors_streaming_list = _client_connect_id_streaming_table.find(client_connect_id)->second;
      for(unsigned int loop_idx = 0 ; loop_idx < sensors_streaming_list.size(); loop_idx++) {
        if(sensors_streaming_list.at(loop_idx) == sensor_handle) {
          sensors_streaming_list.erase(sensors_streaming_list.begin() + loop_idx);
          break;
        }
      }
      if(0 == sensors_streaming_list.size()) {
        delete_qsh_interface_info(interface_instance);

        if( _interface_map_table.end() != _interface_map_table.find(client_connect_id))
          _interface_map_table.erase(client_connect_id);
        else
          sns_loge(" client_connect_id NOT found in _interface_map_table" );

        if( _diag_map_table.end() != _diag_map_table.find(client_connect_id))
          _diag_map_table.erase(client_connect_id);
        else
          sns_loge(" client_connect_id NOT found in _diag_map_table" );

        if( _callback_map_table.end() != _callback_map_table.find(client_connect_id))
          _callback_map_table.erase(client_connect_id);
        else
          sns_loge(" client_connect_id NOT found in _callback_map_table" );

        _client_connect_id_streaming_table.erase(client_connect_id);
      }
    } else {
      sns_logi("USE_CLIENT_CONNECT_ID with normal request");

      bool is_sensor_handle_present = false;
      auto sensors_streaming_list = _client_connect_id_streaming_table.find(client_connect_id)->second;
      for(unsigned int loop_idx = 0 ; loop_idx < sensors_streaming_list.size(); loop_idx++) {
        if(sensors_streaming_list.at(loop_idx) == sensor_handle) {
          is_sensor_handle_present = true;
          break;
        }
      }
      if(false == is_sensor_handle_present) {
        sns_logi("USE_CLIENT_CONNECT_ID new sensor request on the given connection. So updating map table");
        _client_connect_id_streaming_table.find(client_connect_id)->second.push_back(sensor_handle);
      } else {
        sns_logi("USE_CLIENT_CONNECT_ID Reconfiguration request , map table  not updated");
      }
    }
  } else {
    sns_loge(" wrong input from client" );
    return -1;
  }
  sns_logi("send_request for client_connect_id is received - End with client_connect_id %d", client_connect_id);
  return client_connect_id;
}

bool SensorCxt::guard_sample_rate(send_req_msg_info& request_msg)
{
  int sample_rate=0;
  for(auto loop_idx = request_msg.fields.begin(); loop_idx != request_msg.fields.end(); ++loop_idx)
  {
    if(loop_idx->name.compare("sample_rate")==0)
    {
      sample_rate = stoi(loop_idx->value[0]);
      break;
    }
  }
  return (sample_rate <= (int)sensor_attribute::get_max_combined_sample_rate());
}

int SensorCxt::send_request(ISession* interface_instance,
    sensors_diag_log* diag,
    Sensor* current_sensor,
    send_req_msg_info& request_msg,
    client_req_msg_fileds &std_fields_data,
    string& log_file_name) {
  if(!guard_sample_rate(request_msg)){
    sns_loge("Requested sample rate exceeded max supported sample rate.");
    return -1;
  }
  return (current_sensor->send_request(interface_instance, diag, request_msg, std_fields_data, log_file_name));
}

void SensorCxt::delete_qsh_interface_info(ISession *qsh_session){

  int interface_index;
  for(interface_index = 0 ; interface_index < _qsh_interface_info_array.size(); interface_index++) {
    if(_qsh_interface_info_array.at(interface_index).interface == qsh_session) {
      break;
    }
  }

  if(interface_index >= _qsh_interface_info_array.size()) {
    sns_loge(" not able to clear qsh_session position");
    return;
  }

  if(nullptr != _qsh_interface_info_array.at(interface_index).interface) {
    _qsh_interface_info_array.at(interface_index).interface->close();
    delete _qsh_interface_info_array.at(interface_index).interface;
    _qsh_interface_info_array.at(interface_index).interface = nullptr;
  }

  if(nullptr != _qsh_interface_info_array.at(interface_index).diag_log) {
    delete _qsh_interface_info_array.at(interface_index).diag_log;
    _qsh_interface_info_array.at(interface_index).diag_log = nullptr;
  }

  _qsh_interface_info_array.erase(_qsh_interface_info_array.begin() + interface_index);
}

ISession* SensorCxt::create_qsh_interface(int hub_id)
{
  ISession* qsh_session = _session_factory_instance->getSession(hub_id);
  if(nullptr == qsh_session){
    printf("failed to create session interface");
    return nullptr;
  }
  int ret = qsh_session->open();
  if(-1 == ret) {
    sns_loge("error while opening the session");
    delete qsh_session;
    qsh_session = nullptr;
    return nullptr;
  }

  sensors_diag_log* diag = new sensors_diag_log();
  if(nullptr == diag) {
    sns_loge(" failed to create diag" );
    qsh_session->close();
    delete qsh_session;
    qsh_session = nullptr;
    return nullptr;
  }

  qsh_interface_info qsh_interface_info_temp;
  qsh_interface_info_temp.interface = qsh_session;
  qsh_interface_info_temp.diag_log = diag;

  _qsh_interface_info_array.push_back(qsh_interface_info_temp);
  return qsh_session;
}

pair<Sensor*, int> SensorCxt::get_sensor_data(unsigned int required_handle) {
  unsigned int current_handle =  0;
  for(auto sensorptr_it = _sensors_hub_id_map_table.begin(); sensorptr_it != _sensors_hub_id_map_table.end();
      sensorptr_it++,current_handle++ ) {
    if (required_handle == current_handle ) {
      return *sensorptr_it;
    }
  }
  return make_pair(nullptr, -1);
}

int SensorCxt::get_sensor_handle(Sensor* required_sensor) {
  int sensor_handle=0;
  for(auto current_sensor = _sensors_hub_id_map_table.begin(); current_sensor != _sensors_hub_id_map_table.end();
      current_sensor++,sensor_handle++ ) {
    if (required_sensor == current_sensor->first) {
      return sensor_handle;
    }
  }
  return -1;
}

sensors_diag_log* SensorCxt::get_diag_ptr(ISession *interface_instance)
{
  sns_logd("get_diag_ptr start");
  sns_logd("size of current _qsh_interface_info_array is %zu", _qsh_interface_info_array.size());
  unsigned int loop_index =0;
  for(; loop_index < _qsh_interface_info_array.size(); loop_index++) {
    sns_logd("current loop index is %u", loop_index);
    if(_qsh_interface_info_array.at(loop_index).interface == interface_instance) {
      sns_logd("Found the correct one with index is %u", loop_index);
      break;
    }
  }
  if(loop_index >=_qsh_interface_info_array.size()) {
    sns_loge("get_diag_ptr ,interface_instance not found");
    return nullptr;
  }
  return _qsh_interface_info_array.at(loop_index).diag_log;
}

void SensorCxt::process_sensor_event(ISession* conn, string sensor_event, void* ptr) {
  if(nullptr == _cb_thread || sensor_event.empty()) {
    sns_loge("process_sensor_event _cb_thread || sensor_event is empty");
    return;
  }
  lock_guard<mutex> lk(_sync_mutex);
  _cb_thread->add_task([this, conn, sensor_event, ptr]{
    unique_lock<mutex> map_lock(_map_mutex);
    int sensor_handle = this->get_sensor_handle((Sensor*)ptr);
    if(sensor_handle < 0 )
      return;
    int client_connect_id = get_key(conn);
    if(client_connect_id <  0)
      return;

    if(_callback_map_table.end() == _callback_map_table.find(client_connect_id)) {
      sns_loge("call back not properly registered. So dropping the sample here itself");
      return;
    }
    sns_logd("process_sensor_event, with [%d] on client_connect_id [%d]", sensor_handle, client_connect_id);
    sns_logd("%s", sensor_event.c_str());
    register_cb cb = _callback_map_table.find(client_connect_id)->second;
    map_lock.unlock();
    if(nullptr != cb.event_cb)
        cb.event_cb(sensor_event, sensor_handle, client_connect_id);
    else if(nullptr != ptr_display_samples_cb)
      ptr_display_samples_cb(sensor_event, false);
  });
}

void SensorCxt::process_sensor_error(ISession::error sensor_error) {
  sns_loge("%s : Cleaning up the memory" , __func__);
  lock_guard<mutex> lk(_sync_mutex);
   _qsh_interface_info_array.clear();
   _interface_map_table.clear();
   _diag_map_table.clear();
   _callback_map_table.clear();
   _client_connect_id_streaming_table.clear();
}

int SensorCxt::get_key(ISession* conn) {
  for(auto itr = _interface_map_table.begin(); itr!= _interface_map_table.end(); ++itr) {
    if(itr->second == conn)
      return itr->first;
  }
  return -1;
}

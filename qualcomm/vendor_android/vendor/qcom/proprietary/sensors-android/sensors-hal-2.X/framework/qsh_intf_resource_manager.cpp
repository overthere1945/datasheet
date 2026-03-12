/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "qsh_intf_resource_manager.h"
#include "sensor_factory.h"
#include "sensors_log.h"

#define SHARED_ON_CHANGE_MAP_SIZE 2

using namespace std;
using namespace ::com::quic::sensinghub::session::V1_0;

qsh_intf_resource_manager::qsh_intf_resource_manager():
            _current_sensor_id(0)
{
  for(auto hub_id : sensor_factory::instance().get_supported_hub_ids())
  {
    vector<shared_instance_sensors_map> map;
    map.resize(SHARED_ON_CHANGE_MAP_SIZE);
    map[SHARED_ON_CHANGE_NO_WAKEUP].session = nullptr;
    map[SHARED_ON_CHANGE_WAKEUP].session = nullptr;
    _shared_instance_map.insert({hub_id, map});
  }

  _session_factory_instance = make_unique<sessionFactory>();
  if(nullptr == _session_factory_instance){
    printf("failed to create factory instance");
  }

  sns_logd("%s" , __func__);
}

qsh_intf_resource_manager::~qsh_intf_resource_manager()
{
  for(auto hub_id : sensor_factory::instance().get_supported_hub_ids())
  {
    if(nullptr != _shared_instance_map.at(hub_id)[SHARED_ON_CHANGE_NO_WAKEUP].session) {
      delete _shared_instance_map.at(hub_id)[SHARED_ON_CHANGE_NO_WAKEUP].session;
      _shared_instance_map.at(hub_id)[SHARED_ON_CHANGE_NO_WAKEUP].session = nullptr;
    }

    if(nullptr != _shared_instance_map.at(hub_id)[SHARED_ON_CHANGE_WAKEUP].session) {
      delete _shared_instance_map.at(hub_id)[SHARED_ON_CHANGE_WAKEUP].session;
      _shared_instance_map.at(hub_id)[SHARED_ON_CHANGE_WAKEUP].session = nullptr;
    }
  }
  if(nullptr != _session_factory_instance) {
  _session_factory_instance.reset();
  _session_factory_instance = nullptr;
  }
  sns_logd("%s" , __func__);
}

qsh_intf_resource_manager& qsh_intf_resource_manager::get_instance()
{
  static qsh_intf_resource_manager instance;
  return instance;
}

int qsh_intf_resource_manager::get_default_hub_id()
{
#ifdef SNS_WEARABLES_TARGET
    return SENSING_HUB_2;
#else
    return SENSING_HUB_1;
#endif
}
unsigned int qsh_intf_resource_manager::register_sensor(sensor_t sensor_info, int hub_id)
{
  lock_guard<mutex> lk(_mutex);
  _current_sensor_id++;
  conn_type type;

  if(-1 == hub_id) {
    hub_id = get_default_hub_id();
  }

  if(SENSOR_FLAG_CONTINUOUS_MODE == (sensor_info.flags & SENSOR_FLAG_MASK_REPORTING_MODE)) {
    type = UNIQUE_STREAM;
  } else {
    if(true == (sensor_info.flags & SENSOR_FLAG_WAKE_UP)) {
      type = SHARED_ON_CHANGE_WAKEUP;
    } else {
      type = SHARED_ON_CHANGE_NO_WAKEUP;
    }
  }

  _registered_sensor_map[_current_sensor_id] = (pair<int, conn_type>(hub_id, type));
  sns_logd("%s: sensor.type/qsh_intf_sensor_handle[%d/%d], Connection Type[%d]", __func__, sensor_info.type,_current_sensor_id, type);
  return _current_sensor_id;
}

ISession* qsh_intf_resource_manager::acquire(int sensor_id)
{
  sns_logd("%s" , __func__);
  lock_guard<mutex> lk(_mutex);
  auto it = _registered_sensor_map.find(sensor_id);
  if(it != _registered_sensor_map.end()) {
    ISession* session = acquire_session_interface(sensor_id, it->second.second);
      sns_logd("%s completed" , __func__);
      return session;
  } else {
    sns_loge("invalid sensor_id passed to factory - Acquire failed");
    return nullptr;
  }
}

void qsh_intf_resource_manager::release(int sensor_id, ISession* session)
{
  sns_logd("%s" , __func__);
  lock_guard<mutex> lk(_mutex);
  auto it = _registered_sensor_map.find(sensor_id);
  if(it != _registered_sensor_map.end()) {
    release_session_interface(sensor_id, it->second.second, session);
    sns_logd("%s completed" , __func__);
  }else {
    sns_loge("invalid sensor_id passed to factory - Release");
    return;
  }
}

ISession* qsh_intf_resource_manager::acquire_session_interface(int sensor_id, conn_type type)
{
  sns_logi("%s with sensor_id %d , conn_type %d " , __func__, sensor_id, type);
  switch(type) {
    case SHARED_ON_CHANGE_WAKEUP:
      sns_logi("returning _shared_instance[SHARED_ON_CHANGE_WAKEUP].session - wakeup ");
      return acquire_shared_instance(sensor_id, SHARED_ON_CHANGE_WAKEUP);
    case SHARED_ON_CHANGE_NO_WAKEUP:
      sns_logi("returning _shared_instance[SHARED_ON_CHANGE_NO_WAKEUP].session - non-wakeup");
      return acquire_shared_instance(sensor_id, SHARED_ON_CHANGE_NO_WAKEUP);
    case UNIQUE_STREAM:
      return acquire_unique_instance(sensor_id);
    default:
      sns_loge("invalid interface instance type");
      return nullptr;
  }
}

ISession* qsh_intf_resource_manager::acquire_shared_instance(int sensor_id, conn_type type)
{
  int hub_id = _registered_sensor_map.at(sensor_id).first;
  if(nullptr == _shared_instance_map.at(hub_id)[type].session){
    sns_logi("Create new Connection for type %d ", type);
    _shared_instance_map[hub_id][type].session = _session_factory_instance->getSession(hub_id);
    if(nullptr == _shared_instance_map.at(hub_id)[type].session) {
      sns_loge("failed to create instance for qsh connection");
      return nullptr;
    }
    int ret = _shared_instance_map[hub_id][type].session->open();
    if(-1 == ret){
      sns_loge("failed to open session ");
      delete _shared_instance_map[hub_id][type].session;
      _shared_instance_map[hub_id][type].session = nullptr;
      return nullptr;
    }
    _shared_instance_map[hub_id][type].sensors.push_back(sensor_id);
  } else {
    sns_logi("re-use existing connection for type %d ", type);
    bool is_sensor_already_active = false;
    for(unsigned int index = 0; index < _shared_instance_map[hub_id][type].sensors.size() ; index++) {
      if(sensor_id == _shared_instance_map.at(hub_id)[type].sensors.at(index)) {
        is_sensor_already_active = true;
        break;
      }
    }
    if(false == is_sensor_already_active) {
      _shared_instance_map[hub_id][type].sensors.push_back(sensor_id);
    }
  }
  return _shared_instance_map.at(hub_id)[type].session;
}

ISession* qsh_intf_resource_manager::acquire_unique_instance(int sensor_id)
{
  auto it = _unique_instance_map.find(sensor_id);
  if(it == _unique_instance_map.end()) {
    ISession *session = _session_factory_instance->getSession(_registered_sensor_map[sensor_id].first);
    if(nullptr == session) {
      sns_logi("failed to create instance for ISession");
      return nullptr;
    }
    int ret = session->open();
    if(-1 == ret){
      sns_loge("failed to open session ");
      delete session;
      session = nullptr;
      return nullptr;
    }
    unique_instance_sensors_map temp;
    temp.session = session;
    temp.ref_count = 1;
    _unique_instance_map.insert(pair<unsigned int, unique_instance_sensors_map>(sensor_id, temp));
    return session;
  } else {
    /* Unique instance is already created and shared with client
     * but client requested for one more aquire.
     * So just pass the same instance by simply increase the reference count */
    it->second.ref_count++;
  }
  return it->second.session;
}

void qsh_intf_resource_manager::release_session_interface(int sensor_id, conn_type type, ISession *session)
{
  switch(type) {
    case SHARED_ON_CHANGE_WAKEUP:
      release_shared_instance(sensor_id, type);
      break;
    case SHARED_ON_CHANGE_NO_WAKEUP:
      release_shared_instance(sensor_id, type);
      break;
    case UNIQUE_STREAM:
      release_unique_instance(sensor_id);
      break;
    default :
      sns_loge("invalid interface instance type");
      return;
  }
}

void qsh_intf_resource_manager::release_shared_instance(int sensor_id, conn_type type)
{
  int hub_id = _registered_sensor_map.at(sensor_id).first;

  for(unsigned int index = 0; index < _shared_instance_map.at(hub_id)[type].sensors.size(); index++){
    if(sensor_id == _shared_instance_map.at(hub_id)[type].sensors.at(index)) {
      _shared_instance_map.at(hub_id)[type].sensors.erase(_shared_instance_map.at(hub_id)[type].sensors.begin() + index);
      break;
    }
  }
  if(0 == _shared_instance_map.at(hub_id)[type].sensors.size()) {
    if(nullptr != _shared_instance_map.at(hub_id)[type].session) {
      _shared_instance_map.at(hub_id)[type].session->close();
      delete _shared_instance_map.at(hub_id)[type].session;
      _shared_instance_map.at(hub_id)[type].session = nullptr;
    }
  }
}

void qsh_intf_resource_manager::release_unique_instance(int sensor_id)
{
  auto it = _unique_instance_map.find(sensor_id);
  if(it != _unique_instance_map.end()) {
    uint32_t count = it->second.ref_count;
    count--;
    if(count == 0) {
      it->second.session->close();
      delete it->second.session;
      _unique_instance_map.erase(it);
    } else {
      it->second.ref_count = count;
    }
  } else {
    sns_loge("invalid sensor_id");
  }
}

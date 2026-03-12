/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
 
#pragma once
#include "ISession.h"
#include "SessionFactory.h"
#include "sensor.h"
#include <unordered_map>
#include <vector>

#define SENSING_HUB_1 1
#define SENSING_HUB_2 2

class qsh_intf_resource_manager {
public:
  static qsh_intf_resource_manager& get_instance();
  unsigned int register_sensor(sensor_t sensor_info, int hub_id = -1);
  ISession* acquire(int sensor_id);
  void release(int sensor_id, ISession* session);

  qsh_intf_resource_manager(const qsh_intf_resource_manager &) = delete;
  qsh_intf_resource_manager & operator = (const qsh_intf_resource_manager &) = delete;

private:
  enum conn_type{
    SHARED_ON_CHANGE_NO_WAKEUP = 0,
    SHARED_ON_CHANGE_WAKEUP,
    UNIQUE_STREAM
  };

  typedef struct shared_instance_sensors_map{
    ISession* session;
    std::vector<unsigned int> sensors;
  }shared_instance_sensors_map;

  typedef struct unique_instance_sensors_map{
    ISession* session;
    uint32_t ref_count;
  }unique_instance_sensors_map;

  qsh_intf_resource_manager();
  ~qsh_intf_resource_manager();
  ISession* acquire_shared_instance(int sensor_id, conn_type type);
  void release_session_interface(int sensor_id, conn_type type,ISession *session);
  ISession* acquire_unique_instance(int sensor_id);
  ISession *acquire_session_interface(int sensor_id, conn_type type);
  void release_shared_instance(int sensor_id, conn_type type);
  void release_unique_instance(int sensor_id);
  int get_default_hub_id();

  /*  SHARED_ON_CHANGE_NO_WAKEUP - on change non-wakeup shared instance
   *  SHARED_ON_CHANGE_WAKEUP - on change wakeup shared instance*/
  std::unordered_map<int, std::vector<shared_instance_sensors_map>> _shared_instance_map;
  std::unordered_map<unsigned int, unique_instance_sensors_map> _unique_instance_map;
  std::unordered_map<unsigned int, std::pair<int, conn_type>> _registered_sensor_map;
  unsigned int _current_sensor_id;
  std::mutex _mutex;
  std::unique_ptr<sessionFactory> _session_factory_instance;
};

/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#include <vector>
#include <sys/stat.h>

#include "SessionFactory.h"
#include "qmiSession.h"
#ifdef SNS_WEARABLES_TARGET
#include "glinkSession.h"
#endif
#include "sensors_json_parser.h"

using namespace ::com::quic::sensinghub::session::V1_0::implementation;
using namespace std;

#define SENSING_HUB_1   1
#define SENSING_HUB_2   2

#ifdef SNS_WEARABLES_TARGET
#define CONFIG_FILE_PATH "/vendor/etc/sensors/hub1/config/sensing_hub_info.json"
#else
#define CONFIG_FILE_PATH "/vendor/etc/sensors/config/sensing_hub_info.json"
#endif

extern "C"
{
  ISession* getSession(int hub_id);
  void* getSensingHubIds();
}

int getDefaultHubId()
{
  struct stat buf;
  if(0 != stat(CONFIG_FILE_PATH, &buf))
  {
      /* by default, transport layer corresponds to qmi, thus, returning value as 1 */
      return SENSING_HUB_1;
  }
  sensors_json_parser::get_instance().load_file();
#ifdef SNS_WEARABLES_TARGET
  return sensors_json_parser::get_instance().get_hub_id("hub_2");
#endif
  return sensors_json_parser::get_instance().get_hub_id("hub_1");
}

ISession* getSession(int hub_id)
{
  if(-1 == hub_id)
  {
    hub_id = getDefaultHubId();
  }
  switch(hub_id)
  {
    case SENSING_HUB_1:
      try
      {
        sns_logi("sessionImpl: creating qmi session");
        return new qmiSession();
      }
      catch(const exception& e)
      {
        sns_loge("sessionImpl: %s failed to create qmi session: %s", e.what());
        return nullptr;
      }
      break;

    case SENSING_HUB_2:
      try
      {
        sns_logi("sessionImpl: Creating glink session");
#ifdef SNS_WEARABLES_TARGET
        return new glinkSession(sensors_json_parser::get_instance().get_comm_handle_attrs(hub_id), hub_id);
#else
        return nullptr;
#endif
      }
      catch(const exception& e)
      {
        sns_loge("sessionImpl: %s failed to create glink session: %s", e.what());
        return nullptr;
      }
      break;

    default:
      sns_loge("hub id %d not supported in ISession", hub_id);
      break;
  }
  return nullptr;
}

void* getSensingHubIds()
{
  std::vector<int>* hub_list = new std::vector<int>();
  struct stat buf;
  if(0 == stat(CONFIG_FILE_PATH, &buf))
  {
    sensors_json_parser parser_instance;
    sensors_json_parser::get_instance().load_file();
    vector<int> hub_ids = sensors_json_parser::get_instance().get_hub_id_list();
    for(auto hub_id : hub_ids)
      hub_list->push_back(hub_id);
  }
  else{
      hub_list->push_back(getDefaultHubId());
  }
  return hub_list;
}

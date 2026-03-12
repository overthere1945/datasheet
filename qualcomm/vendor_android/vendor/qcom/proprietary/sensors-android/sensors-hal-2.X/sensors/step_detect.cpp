/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <cinttypes>
#include "sensor_factory.h"
#include "qsh_sensor.h"
#include "sns_step_detect.pb.h"

using namespace std;

static const char *QSH_DATATYPE_STEP_DETECT = "step_detect";

static const uint32_t STEP_DETECT_RESERVED_FIFO_COUNT = 300;

class step_detect : public qsh_sensor
{
public:
  step_detect(suid sensor_uid, sensor_wakeup_type wakeup);

private:
  virtual void handle_sns_client_event(
      const sns_client_event_msg_sns_client_event& pb_event) override;
};

step_detect::step_detect(suid sensor_uid, sensor_wakeup_type wakeup):
    qsh_sensor(sensor_uid, wakeup)
{
  set_type(SENSOR_TYPE_STEP_DETECTOR);
  set_string_type(SENSOR_STRING_TYPE_STEP_DETECTOR);
  set_fifo_reserved_count(STEP_DETECT_RESERVED_FIFO_COUNT);
  set_reporting_mode(SENSOR_FLAG_SPECIAL_REPORTING_MODE);
  set_nowk_msgid(SNS_STEP_DETECT_MSGID_SNS_STEP_DETECT_EVENT);
  if(false == is_resolution_set)
    set_resolution(1.0f);
  if(false == is_max_range_set)
    set_max_range(1.0f);
  _qsh_intf_id = qsh_intf_resource_manager::get_instance().register_sensor(get_sensor_info());
}

void step_detect::handle_sns_client_event(
        const sns_client_event_msg_sns_client_event& pb_event)
{
  auto msg_id = pb_event.msg_id();

  if (SNS_STEP_DETECT_MSGID_SNS_STEP_DETECT_EVENT == msg_id) {
    sns_logd("step_event is coming from step_detect");
    Event hal_event =
        create_sensor_hal_event(pb_event.timestamp());
    hal_event.u.scalar = 1.0f;
    if(true == can_submit_sample(hal_event))
      events.push_back(hal_event);
    sns_logv("step_detect_event: ts=%" PRIu64, hal_event.timestamp);
  } else {
    sns_logi("unknown message %u is coming", msg_id);
  }
}

static vector<unique_ptr<sensor>> get_available_step_detect_sensors()
{
  vector<unique_ptr<sensor>> sensors;
  const vector<suid>& suids =
        sensor_factory::instance().get_suids(QSH_DATATYPE_STEP_DETECT);

  for (const auto& sensor_uid : suids) {
    try {
      sensors.push_back(make_unique<step_detect>(sensor_uid, SENSOR_WAKEUP));
    } catch (const exception& e) {
      sns_loge("%s", e.what());
      sns_loge("failed to create sensor step_detect, wakeup=true");
    }
    try {
      sensors.push_back(make_unique<step_detect>(sensor_uid, SENSOR_NO_WAKEUP));
    } catch (const exception& e) {
      sns_loge("%s", e.what());
      sns_loge("failed to create sensor step_detect, wakeup=false");
    }
  }
  return sensors;
}

static bool step_detect_module_init()
{
  /* register supported sensor types with factory */
  sensor_factory::register_sensor(SENSOR_TYPE_STEP_DETECTOR,
      get_available_step_detect_sensors);
  sensor_factory::request_datatype(QSH_DATATYPE_STEP_DETECT);
  return true;
}

SENSOR_MODULE_INIT(step_detect_module_init);

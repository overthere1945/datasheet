/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <cinttypes>
#include <string>
#include "sensor_factory.h"
#include "qsh_sensor.h"
#include "sns_persist_stationary_detect.pb.h"

static const char *QSH_DATATYPE_STATIONARY_DETECT = "persist_stationary_detect";

class stationary_detect : public qsh_sensor
{
public:
    stationary_detect(suid sensor_uid, sensor_wakeup_type wakeup);
    static const char* qsh_datatype() { return QSH_DATATYPE_STATIONARY_DETECT; }

private:
    virtual void handle_sns_client_event(
        const sns_client_event_msg_sns_client_event& pb_event) override;
};

stationary_detect::stationary_detect(suid sensor_uid, sensor_wakeup_type wakeup):
    qsh_sensor(sensor_uid, wakeup)
{
    set_type(SENSOR_TYPE_STATIONARY_DETECT);
    set_string_type(SENSOR_STRING_TYPE_STATIONARY_DETECT);
    if (wakeup)
        set_name("stationary_detect_wakeup");
    else
        set_name("stationary_detect");
    set_reporting_mode(SENSOR_FLAG_ONE_SHOT_MODE);
    // One-shot sensors must report "-1" for the minDelay
    set_min_delay(-1);
    set_fifo_max_count(0);
    if(false == is_resolution_set)
        set_resolution(1.0f);
    if(false == is_max_range_set)
        set_max_range(1.0f);
    _is_one_shot_senor = true;
    _qsh_intf_id = qsh_intf_resource_manager::get_instance().register_sensor(get_sensor_info());
}


void stationary_detect::handle_sns_client_event(
        const sns_client_event_msg_sns_client_event& pb_event)
{
    if (!_oneshot_done && SNS_PERSIST_STATIONARY_DETECT_MSGID_SNS_PSD_EVENT == pb_event.msg_id()) {
        Event hal_event = create_sensor_hal_event(pb_event.timestamp());
        hal_event.u.scalar = 1.0f;
        if(true == can_submit_sample(hal_event))
          events.push_back(hal_event);
        sns_logv("staionary_motion_event: ts=%" PRIi64,
                hal_event.timestamp);
    }
}

static bool stationary_detect_module_init()
{
    /* register supported sensor types with factory */
    sensor_factory::register_sensor(SENSOR_TYPE_STATIONARY_DETECT,
        qsh_sensor::get_available_sensors<stationary_detect>);
    sensor_factory::request_datatype(QSH_DATATYPE_STATIONARY_DETECT);
    return true;
}

SENSOR_MODULE_INIT(stationary_detect_module_init);

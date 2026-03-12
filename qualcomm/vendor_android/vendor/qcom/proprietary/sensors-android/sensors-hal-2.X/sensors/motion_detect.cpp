/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <cinttypes>
#include <string>
#include "sensor_factory.h"
#include "qsh_sensor.h"
#include "sns_persist_motion_detect.pb.h"

static const char *QSH_DATATYPE_MOTION_DETECT = "persist_motion_detect";


class motion_detect : public qsh_sensor
{
public:
    motion_detect(suid sensor_uid, sensor_wakeup_type wakeup);
    static const char* qsh_datatype() { return QSH_DATATYPE_MOTION_DETECT; }

private:
    virtual void handle_sns_client_event(
        const sns_client_event_msg_sns_client_event& pb_event) override;
};

motion_detect::motion_detect(suid sensor_uid, sensor_wakeup_type wakeup):
    qsh_sensor(sensor_uid, wakeup)
{
    set_type(SENSOR_TYPE_MOTION_DETECT);
    set_string_type(SENSOR_STRING_TYPE_MOTION_DETECT);
    if (wakeup)
        set_name("motion_detect_wakeup");
    else
        set_name("motion_detect");
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

void motion_detect::handle_sns_client_event(
        const sns_client_event_msg_sns_client_event& pb_event)
{
    if (!_oneshot_done && SNS_PERSIST_MOTION_DETECT_MSGID_SNS_PMD_EVENT == pb_event.msg_id()) {
        Event hal_event =
            create_sensor_hal_event(pb_event.timestamp());

        hal_event.u.scalar = 1.0f;

        if(true == can_submit_sample(hal_event))
          events.push_back(hal_event);

        sns_logv("motion_detect_event: ts=%" PRIi64,
                hal_event.timestamp);

    }
}

static bool motion_detect_module_init()
{
    /* register supported sensor types with factory */
    sensor_factory::register_sensor(SENSOR_TYPE_MOTION_DETECT,
        qsh_sensor::get_available_sensors<motion_detect>);
    sensor_factory::request_datatype(QSH_DATATYPE_MOTION_DETECT);
    return true;
}

SENSOR_MODULE_INIT(motion_detect_module_init);

/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#ifdef SNS_WEARABLES_TARGET

#include <cinttypes>
#include "sensor_factory.h"
#include "qsh_sensor.h"
#include "sns_offbody_detect.pb.h"

static const char *QSH_DATATYPE_OFFBODY_DETECT = "offbody_detect";

class offbody_detect : public qsh_sensor
{
public:
    offbody_detect(suid sensor_uid, sensor_wakeup_type wakeup);
    static const char* qsh_datatype() { return QSH_DATATYPE_OFFBODY_DETECT; }

private:
    virtual void handle_sns_client_event(
        const sns_client_event_msg_sns_client_event& pb_event) override;
};

offbody_detect::offbody_detect(suid sensor_uid, sensor_wakeup_type wakeup):
    qsh_sensor(sensor_uid, wakeup)
{
    set_type(SENSOR_TYPE_LOW_LATENCY_OFFBODY_DETECT);
    set_string_type(SENSOR_STRING_TYPE_LOW_LATENCY_OFFBODY_DETECT);
    if(false == is_resolution_set)
        set_resolution(1.0f);
    if(false == is_max_range_set)
        set_max_range(1.0f);
    _qsh_intf_id = qsh_intf_resource_manager::get_instance().register_sensor(get_sensor_info());
}

void offbody_detect::handle_sns_client_event(
        const sns_client_event_msg_sns_client_event& pb_event)
{
    if (SNS_OFFBODY_DETECT_MSGID_SNS_OFFBODY_DETECT_EVENT == pb_event.msg_id()) {
        sns_offbody_detect_event pb_offbody_detect_event;
        pb_offbody_detect_event.ParseFromString(pb_event.payload());

        Event hal_event =
             create_sensor_hal_event(pb_event.timestamp());

        if (SNS_OFFBODY_DETECT_EVENT_TYPE_ON ==
              pb_offbody_detect_event.state()) {
            hal_event.u.scalar = 1.0f;
        } else {
            hal_event.u.scalar = 0.0f;
        }

        if(true == can_submit_sample(hal_event))
          events.push_back(hal_event);

        sns_logv("offbody_detect_event: ts=%" PRIu64 ", type=%i",
                 hal_event.timestamp,
                 pb_offbody_detect_event.state());
    }
}

static bool offbody_detect_module_init()
{
    /* register supported sensor types with factory */
    sensor_factory::register_sensor(SENSOR_TYPE_LOW_LATENCY_OFFBODY_DETECT,
        qsh_sensor::get_available_sensors<offbody_detect>);
    sensor_factory::request_datatype(QSH_DATATYPE_OFFBODY_DETECT);
    return true;
}

SENSOR_MODULE_INIT(offbody_detect_module_init);
#endif
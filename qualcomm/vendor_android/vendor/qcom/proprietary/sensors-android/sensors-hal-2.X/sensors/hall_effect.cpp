/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "sensor_factory.h"
#include "qsh_sensor.h"
#include "sensors_qti.h"
#include "sns_hall.pb.h"

static const char *QSH_DATATYPE_HALL_EFFECT = "hall";

class hall_effect : public qsh_sensor
{
public:
    hall_effect(suid sensor_uid, sensor_wakeup_type wakeup);
    static const char* qsh_datatype() { return QSH_DATATYPE_HALL_EFFECT; }
private:
    virtual void handle_sns_client_event(
        const sns_client_event_msg_sns_client_event& pb_event) override;
};

hall_effect::hall_effect(suid sensor_uid, sensor_wakeup_type wakeup):
    qsh_sensor(sensor_uid, wakeup)
{
    set_type(QTI_SENSOR_TYPE_HALL_EFFECT);
    set_string_type(QTI_SENSOR_STRING_TYPE_HALL_EFFECT);
    set_sensor_typename("Hall Effect Sensor");
    set_nowk_msgid(SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_PHYSICAL_CONFIG_EVENT);
    if(false == is_resolution_set)
        set_resolution(1.0f);
    if(false == is_max_range_set)
        set_max_range(1.0f);
    _qsh_intf_id = qsh_intf_resource_manager::get_instance().register_sensor(get_sensor_info());
}

void hall_effect::handle_sns_client_event(
    const sns_client_event_msg_sns_client_event& pb_event)
{
    if (pb_event.msg_id() == SNS_HALL_MSGID_SNS_HALL_EVENT) {
        sns_hall_event pb_hall_event;
        pb_hall_event.ParseFromString(pb_event.payload());

        Event hal_event =
             create_sensor_hal_event(pb_event.timestamp());

        if (pb_hall_event.hall_event_type() == SNS_HALL_EVENT_TYPE_NEAR) {
            hal_event.u.scalar = 0.0f;
        } else {
            hal_event.u.scalar = 1.0f;
        }
        if(true == can_submit_sample(hal_event))
          events.push_back(hal_event);

        sns_logv("hall_event: ts=%llu, near_far=%d, distance=%f",
                 (unsigned long long)hal_event.timestamp,
                 pb_hall_event.hall_event_type(),
                 hal_event.u.scalar);
    }
}


static bool hall_effect_module_init()
{
    /* register supported sensor types with factory */
    sensor_factory::register_sensor(
        QTI_SENSOR_TYPE_HALL_EFFECT,
        qsh_sensor::get_available_sensors<hall_effect>);
    sensor_factory::request_datatype(QSH_DATATYPE_HALL_EFFECT);
    return true;
}

SENSOR_MODULE_INIT(hall_effect_module_init);

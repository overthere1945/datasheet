/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#ifdef SNS_WEARABLES_TARGET

#include <cinttypes>
#include "sensor_factory.h"
#include "qsh_sensor.h"
#include "sns_tilt.pb.h"

static const char *QSH_DATATYPE_WRIST_TILT_DETECTOR = "wrist_tilt_gesture";

class wrist_tilt : public qsh_sensor
{
public:
    wrist_tilt(suid sensor_uid, sensor_wakeup_type wakeup);
    static const char* qsh_datatype() { return QSH_DATATYPE_WRIST_TILT_DETECTOR; }

private:
    virtual void handle_sns_client_event(
        const sns_client_event_msg_sns_client_event& pb_event) override;
};

wrist_tilt::wrist_tilt(suid sensor_uid, sensor_wakeup_type wakeup):
    qsh_sensor(sensor_uid, wakeup)
{
    set_type(SENSOR_TYPE_WRIST_TILT_GESTURE);
    set_string_type(SENSOR_STRING_TYPE_WRIST_TILT_GESTURE);
    set_reporting_mode(SENSOR_FLAG_SPECIAL_REPORTING_MODE);
    _qsh_intf_id = qsh_intf_resource_manager::get_instance().register_sensor(get_sensor_info());
}

void wrist_tilt::handle_sns_client_event(
        const sns_client_event_msg_sns_client_event& pb_event)
{
    if (SNS_TILT_MSGID_SNS_TILT_EVENT == pb_event.msg_id()) {
        Event hal_event =
             create_sensor_hal_event(pb_event.timestamp());

        hal_event.u.scalar = 1.0f;

        if(true == can_submit_sample(hal_event))
          events.push_back(hal_event);

        sns_logv("wrist_tilt_event: ts=%" PRIi64, hal_event.timestamp);
        if(_stats != NULL)
          _stats->update_sample(hal_event.timestamp, pb_event.timestamp());
    }
}

static bool wrist_tilt_module_init()
{
    /* register supported sensor types with factory */
    sensor_factory::register_sensor(SENSOR_TYPE_WRIST_TILT_GESTURE,
        qsh_sensor::get_available_wakeup_sensors<wrist_tilt>);
    sensor_factory::request_datatype(QSH_DATATYPE_WRIST_TILT_DETECTOR);
    return true;
}

SENSOR_MODULE_INIT(wrist_tilt_module_init);
#endif
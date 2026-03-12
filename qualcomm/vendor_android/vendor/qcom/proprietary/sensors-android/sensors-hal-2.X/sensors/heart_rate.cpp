/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#ifdef SNS_WEARABLES_TARGET

#include "sensor_factory.h"
#include "qsh_sensor.h"
#include "sns_heart_rate.pb.h"

static const char *QSH_DATATYPE_HEART_RATE = "heart_rate";

class heart_rate : public qsh_sensor
{
public:
    heart_rate(suid sensor_uid, sensor_wakeup_type wakeup);
    static const char* qsh_datatype() { return QSH_DATATYPE_HEART_RATE; }

private:
    virtual void handle_sns_client_event(
        const sns_client_event_msg_sns_client_event& pb_event) override;
};

heart_rate::heart_rate(suid sensor_uid, sensor_wakeup_type wakeup):
    qsh_sensor(sensor_uid, wakeup)
{
    set_type(SENSOR_TYPE_HEART_RATE);
    set_string_type(SENSOR_STRING_TYPE_HEART_RATE);
    _qsh_intf_id = qsh_intf_resource_manager::get_instance().register_sensor(get_sensor_info());
}

static bool heart_rate_module_init()
{
    /* register supported sensor types with factory */
    sensor_factory::register_sensor(SENSOR_TYPE_HEART_RATE,
        qsh_sensor::get_available_sensors<heart_rate>);
    sensor_factory::request_datatype(QSH_DATATYPE_HEART_RATE);
    return true;
}

void heart_rate::handle_sns_client_event(
        const sns_client_event_msg_sns_client_event& pb_event)
{
    if (SNS_HEART_RATE_MSGID_SNS_HEART_RATE_EVENT == pb_event.msg_id()) {
        sns_heart_rate_event pb_heart_rate_event;
        pb_heart_rate_event.ParseFromString(pb_event.payload());

        Event hal_event =
             create_sensor_hal_event(pb_event.timestamp());

        hal_event.u.heartRate.status = (SensorStatus)pb_heart_rate_event.heart_rate_event_status();

        if(SensorStatus::NO_CONTACT == hal_event.u.heartRate.status ||
            SensorStatus::UNRELIABLE == hal_event.u.heartRate.status){
          hal_event.u.heartRate.bpm = 0.0;
        } else {
          hal_event.u.heartRate.bpm = pb_heart_rate_event.heart_rate();
        }

        if(true == can_submit_sample(hal_event))
          events.push_back(hal_event);

        sns_logv("heart_rate_event: ts=%" PRIu64 ", value=%f , type=%i",
                 hal_event.timestamp,
                 pb_heart_rate_event.heart_rate(),
                 pb_heart_rate_event.heart_rate_event_status());
    }
}

SENSOR_MODULE_INIT(heart_rate_module_init);
#endif
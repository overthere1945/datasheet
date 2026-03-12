/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <cinttypes>
#include "sensor_factory.h"
#include "qsh_sensor.h"
#include "sns_device_orient.pb.h"

static const char *QSH_DATATYPE_DEVICE_ORIENTATION = "device_orient";

class device_orient : public qsh_sensor
{
public:
    device_orient(suid sensor_uid, sensor_wakeup_type wakeup);
    static const char* qsh_datatype() { return QSH_DATATYPE_DEVICE_ORIENTATION; }

private:
    virtual void handle_sns_client_event(
        const sns_client_event_msg_sns_client_event& pb_event) override;
};

device_orient::device_orient(suid sensor_uid, sensor_wakeup_type wakeup):
    qsh_sensor(sensor_uid, wakeup)
{
    set_type(SENSOR_TYPE_DEVICE_ORIENTATION);
    set_string_type(SENSOR_STRING_TYPE_DEVICE_ORIENTATION);
    if(false == is_resolution_set)
        set_resolution(1.0f);
    if(false == is_max_range_set)
        set_max_range(1.0f);
    _qsh_intf_id = qsh_intf_resource_manager::get_instance().register_sensor(get_sensor_info());
}

void device_orient::handle_sns_client_event(
        const sns_client_event_msg_sns_client_event& pb_event)
{
    if (SNS_DEVICE_ORIENT_MSGID_SNS_DEVICE_ORIENT_EVENT == pb_event.msg_id()) {
        sns_device_orient_event pb_device_orient_event;
        pb_device_orient_event.ParseFromString(pb_event.payload());

        Event hal_event =
             create_sensor_hal_event(pb_event.timestamp());

        hal_event.u.scalar = pb_device_orient_event.state();

        if(true == can_submit_sample(hal_event))
          events.push_back(hal_event);

        sns_logv("device_orient_event: ts=%" PRIu64 ", type=%i",
                 hal_event.timestamp,
                 pb_device_orient_event.state());
    }
}

static bool device_orient_module_init()
{
    /* register supported sensor types with factory */
    sensor_factory::register_sensor(SENSOR_TYPE_DEVICE_ORIENTATION,
        qsh_sensor::get_available_sensors<device_orient>);
    sensor_factory::request_datatype(QSH_DATATYPE_DEVICE_ORIENTATION);
    return true;
}

SENSOR_MODULE_INIT(device_orient_module_init);


/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#ifdef SNS_WEARABLES_TARGET

#include "sensor_factory.h"
#include "qsh_sensor.h"
#include "sns_heart_beat.pb.h"

static const char *QSH_DATATYPE_HEART_BEAT = "heart_beat";

class heart_beat : public qsh_sensor
{
public:
    heart_beat(suid sensor_uid, sensor_wakeup_type wakeup);
    static const char* qsh_datatype() { return QSH_DATATYPE_HEART_BEAT; }
};

heart_beat::heart_beat(suid sensor_uid, sensor_wakeup_type wakeup):
    qsh_sensor(sensor_uid, wakeup)
{
    set_type(SENSOR_TYPE_HEART_BEAT);
    set_string_type(SENSOR_STRING_TYPE_HEART_BEAT);
    _qsh_intf_id = qsh_intf_resource_manager::get_instance().register_sensor(get_sensor_info());
}

static bool heart_beat_module_init()
{
    /* register supported sensor types with factory */
    sensor_factory::register_sensor(SENSOR_TYPE_HEART_BEAT,
        qsh_sensor::get_available_sensors<heart_beat>);
    sensor_factory::request_datatype(QSH_DATATYPE_HEART_BEAT);
    return true;
}

SENSOR_MODULE_INIT(heart_beat_module_init);
#endif
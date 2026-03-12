/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "sensor_factory.h"
#include "qsh_sensor.h"
#include "sns_std_sensor.pb.h"
#include "sns_physical_sensor_test.pb.h"

static const char *QSH_DATATYPE_HUMIDITY = "humidity";

class humidity : public qsh_sensor
{
public:
    humidity(suid sensor_uid, sensor_wakeup_type wakeup);
    static const char* qsh_datatype() { return QSH_DATATYPE_HUMIDITY; }
};

humidity::humidity(suid sensor_uid, sensor_wakeup_type wakeup):
    qsh_sensor(sensor_uid, wakeup)
{
    set_type(SENSOR_TYPE_RELATIVE_HUMIDITY);
    set_string_type(SENSOR_STRING_TYPE_RELATIVE_HUMIDITY);
    set_sensor_typename("Humidity Sensor");
    set_nowk_msgid(SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_PHYSICAL_CONFIG_EVENT);
    set_nowk_msgid(SNS_PHYSICAL_SENSOR_TEST_MSGID_SNS_PHYSICAL_SENSOR_TEST_EVENT);
    _qsh_intf_id = qsh_intf_resource_manager::get_instance().register_sensor(get_sensor_info());
}

static bool humidity_module_init()
{
    /* register supported sensor types with factory */
    sensor_factory::register_sensor(
        SENSOR_TYPE_RELATIVE_HUMIDITY,
        qsh_sensor::get_available_sensors<humidity>);
    sensor_factory::request_datatype(QSH_DATATYPE_HUMIDITY);
    return true;
}

SENSOR_MODULE_INIT(humidity_module_init);

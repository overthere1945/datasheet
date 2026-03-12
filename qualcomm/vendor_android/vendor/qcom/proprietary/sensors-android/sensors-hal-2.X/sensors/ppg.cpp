/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifdef SNS_WEARABLES_TARGET

#include <cutils/properties.h>
#include "sensor_factory.h"
#include "qsh_sensor.h"
#include "sns_std_sensor.pb.h"
#include "sns_physical_sensor_test.pb.h"

#define SENSOR_TYPE_PPG                         65572
#define SENSOR_STRING_TYPE_PPG                  "com.google.wear.sensor.ppg"

static const char *QSH_DATATYPE_PPG = "ppg";
static const uint32_t PPG_RESERVED_FIFO_COUNT = 300;

class ppg : public qsh_sensor
{
public:
    ppg(suid sensor_uid, sensor_wakeup_type wakeup);
    static const char* qsh_datatype() { return QSH_DATATYPE_PPG; }
};

ppg::ppg(suid sensor_uid, sensor_wakeup_type wakeup):
    qsh_sensor(sensor_uid, wakeup)
{
    set_type(SENSOR_TYPE_PPG);
    set_string_type(SENSOR_STRING_TYPE_PPG);
    set_sensor_typename("PPG Sensor");
    set_fifo_reserved_count(PPG_RESERVED_FIFO_COUNT);
    set_nowk_msgid(SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_PHYSICAL_CONFIG_EVENT);
    _qsh_intf_id = qsh_intf_resource_manager::get_instance().register_sensor(get_sensor_info());
}

static bool ppg_module_init()
{
    /* register supported sensor types with factory */
    sensor_factory::register_sensor(SENSOR_TYPE_PPG,
        qsh_sensor::get_available_sensors<ppg>);
    sensor_factory::request_datatype(QSH_DATATYPE_PPG);
    return true;
}

SENSOR_MODULE_INIT(ppg_module_init);
#endif
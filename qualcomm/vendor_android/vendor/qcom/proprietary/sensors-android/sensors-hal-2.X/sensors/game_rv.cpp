/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "sensor_factory.h"
#include "qsh_sensor.h"
#include "sns_std_sensor.pb.h"

static const char *QSH_DATATYPE_GAME_RV = "game_rv";

static const uint32_t GAME_RV_RESERVED_FIFO_COUNT = 300;

class game_rv : public qsh_sensor
{
public:
    game_rv(suid sensor_uid, sensor_wakeup_type wakeup);
    static const char* qsh_datatype() { return QSH_DATATYPE_GAME_RV; }
};

game_rv::game_rv(suid sensor_uid, sensor_wakeup_type wakeup):
    qsh_sensor(sensor_uid, wakeup)
{
    set_type(SENSOR_TYPE_GAME_ROTATION_VECTOR);
    set_string_type(SENSOR_STRING_TYPE_GAME_ROTATION_VECTOR);
    set_fifo_reserved_count(GAME_RV_RESERVED_FIFO_COUNT);
    if(false == is_resolution_set)
        set_resolution(0.01f);
    if(false == is_max_range_set)
        set_max_range(1.0f);
    _qsh_intf_id = qsh_intf_resource_manager::get_instance().register_sensor(get_sensor_info());
}

static bool game_rv_module_init()
{
    /* register supported sensor types with factory */
    sensor_factory::register_sensor(SENSOR_TYPE_GAME_ROTATION_VECTOR,
        qsh_sensor::get_available_sensors<game_rv>);
    sensor_factory::request_datatype(QSH_DATATYPE_GAME_RV);
    return true;
}

SENSOR_MODULE_INIT(game_rv_module_init);

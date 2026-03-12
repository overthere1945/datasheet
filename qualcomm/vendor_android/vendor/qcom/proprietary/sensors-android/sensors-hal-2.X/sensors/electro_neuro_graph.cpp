/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#ifdef SNS_WEARABLES_TARGET
#include "sensor_factory.h"
#include "qsh_sensor.h"
#include "sns_std_sensor.pb.h"
#include "sns_physical_sensor_test.pb.h"
#include "sensors_qti.h"

static const char *QSH_DATATYPE_ENG = "electro_neuro_graph";
#define SNS_ENG_HUB_ID 1

class electro_neuro_graph : public qsh_sensor
{
public:
    electro_neuro_graph(suid sensor_uid, sensor_wakeup_type wakeup);
    static const char* qsh_datatype() { return QSH_DATATYPE_ENG; }
};

electro_neuro_graph::electro_neuro_graph(suid sensor_uid, sensor_wakeup_type wakeup):
    qsh_sensor(sensor_uid, wakeup)
{
    set_type(QTI_SENSOR_TYPE_ENG);
    set_string_type(QTI_SENSOR_STRING_TYPE_ENG);
    set_sensor_typename("Electro Neuro Graph Sensor");
    set_nowk_msgid(SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_PHYSICAL_CONFIG_EVENT);
    set_nowk_msgid(SNS_STD_MSGID_SNS_STD_RESAMPLER_CONFIG_EVENT);
    _qsh_intf_id = qsh_intf_resource_manager::get_instance().register_sensor(get_sensor_info(), SNS_ENG_HUB_ID);
}

static vector<unique_ptr<sensor>> get_available_eng_sensors()
{
    vector<unique_ptr<sensor>> sensors;
    const vector<suid>& eng_suid =
            sensor_factory::instance().get_suids(QSH_DATATYPE_ENG, SNS_ENG_HUB_ID);

    for (const auto& sensor_uid : eng_suid) {
        const sensor_attributes& attr =
             sensor_factory::instance().get_attributes(sensor_uid);
        try {
            sensors.push_back(make_unique<electro_neuro_graph>(sensor_uid, SENSOR_WAKEUP));
        } catch (const exception& e) {
            sns_loge("failed for wakeup, %s", e.what());
        }
        try {
            sensors.push_back(make_unique<electro_neuro_graph>(sensor_uid, SENSOR_NO_WAKEUP));
        } catch (const exception& e) {
            sns_loge("failed for nowakeup, %s", e.what());
        }
    }
    return sensors;
}

static bool eng_module_init()
{
    /* register supported sensor types with factory */
    sensor_factory::register_sensor(
        QTI_SENSOR_TYPE_ENG, get_available_eng_sensors);
    sensor_factory::request_datatype(QSH_DATATYPE_ENG);
    return true;
}

SENSOR_MODULE_INIT(eng_module_init);
#endif /* SNS_WEARABLES_TARGET */

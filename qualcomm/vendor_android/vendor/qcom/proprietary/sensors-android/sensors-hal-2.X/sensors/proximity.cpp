/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "sensor_factory.h"
#include "qsh_sensor.h"
#include "sns_proximity.pb.h"

using namespace std;

static const char *QSH_DATATYPE_PROXIMITY = "proximity";
static const uint32_t PROX_RESERVED_FIFO_COUNT = 300;

using namespace std;

class proximity : public qsh_sensor
{
public:
    proximity(suid sensor_uid, sensor_wakeup_type wakeup);
    static const char* qsh_datatype() { return QSH_DATATYPE_PROXIMITY; }

private:
    virtual void handle_sns_client_event(
        const sns_client_event_msg_sns_client_event& pb_event) override;
};

proximity::proximity(suid sensor_uid, sensor_wakeup_type wakeup):
    qsh_sensor(sensor_uid, wakeup)
{
    set_type(SENSOR_TYPE_PROXIMITY);
    set_string_type(SENSOR_STRING_TYPE_PROXIMITY);
    set_fifo_reserved_count(PROX_RESERVED_FIFO_COUNT);
    set_sensor_typename("Proximity Sensor");
    _qsh_intf_id = qsh_intf_resource_manager::get_instance().register_sensor(get_sensor_info());
}

void proximity::handle_sns_client_event(
        const sns_client_event_msg_sns_client_event& pb_event)
{
    if (pb_event.msg_id() == SNS_PROXIMITY_MSGID_SNS_PROXIMITY_EVENT) {
        sns_proximity_event pb_prox_event;
        pb_prox_event.ParseFromString(pb_event.payload());

        if (pb_prox_event.status() != SNS_STD_SENSOR_SAMPLE_STATUS_ACCURACY_HIGH) {
            sns_logi("prox_event: filter out status = %d, raw_adc=%lu, near_far=%d",
                     pb_prox_event.status(),
                     (unsigned long)pb_prox_event.raw_adc(),
                     pb_prox_event.proximity_event_type());
            return;
        }

        Event hal_event =
             create_sensor_hal_event(pb_event.timestamp());

        if (pb_prox_event.proximity_event_type() ==
                SNS_PROXIMITY_EVENT_TYPE_NEAR) {
            hal_event.u.scalar = 0.0f;
        } else {
            hal_event.u.scalar = get_sensor_info().maxRange;
        }

        if(true == can_submit_sample(hal_event))
          events.push_back(hal_event);

        sns_logv("prox_event: ts=%llu, raw_adc=%lu, near_far=%d, distance=%f",
                 (unsigned long long)hal_event.timestamp,
                 (unsigned long)pb_prox_event.raw_adc(),
                 pb_prox_event.proximity_event_type(),
                 hal_event.u.scalar);
    }
}

/*Proximity is default wakeup sensor , so create it*/
static vector<unique_ptr<sensor>> get_available_prox_sensors()
{
    vector<unique_ptr<sensor>> sensors;
    const vector<suid>& prox_suids =
            sensor_factory::instance().get_suids(QSH_DATATYPE_PROXIMITY);

    for (const auto& sensor_uid : prox_suids) {
        const sensor_attributes& attr =
                sensor_factory::instance().get_attributes(sensor_uid);
        /* publish only on-change proximity sensor */
        if (attr.get_stream_type() != SNS_STD_SENSOR_STREAM_TYPE_ON_CHANGE) {
            continue;
        }
        try {
            sensors.push_back(make_unique<proximity>(sensor_uid, SENSOR_WAKEUP));
        } catch (const exception& e) {
            sns_loge("failed for wakeup, %s", e.what());
        }
        try {
            sensors.push_back(make_unique<proximity>(sensor_uid, SENSOR_NO_WAKEUP));
        } catch (const exception& e) {
            sns_loge("failed for nowakeup, %s", e.what());
        }
    }
    return sensors;
}
static bool proximity_module_init()
{
    /* register supported sensor types with factory */
    sensor_factory::register_sensor(SENSOR_TYPE_PROXIMITY,
                                    get_available_prox_sensors);
    sensor_factory::request_datatype(QSH_DATATYPE_PROXIMITY);
    return true;
}

SENSOR_MODULE_INIT(proximity_module_init);

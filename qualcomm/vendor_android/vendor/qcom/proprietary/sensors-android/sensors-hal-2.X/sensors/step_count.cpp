/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <cinttypes>
#include "sensor_factory.h"
#include "qsh_sensor.h"
#include "sns_pedometer.pb.h"

using namespace std;
static const char *QSH_DATATYPE_PEDOMETER = "pedometer";
static const uint32_t PEDOMETER_RESERVED_FIFO_COUNT = 300;

class step_count : public qsh_sensor
{
public:
    step_count(suid sensor_uid, sensor_wakeup_type wakeup);
    static const char* qsh_datatype() { return QSH_DATATYPE_PEDOMETER; }

private:
    virtual void handle_sns_client_event(
        const sns_client_event_msg_sns_client_event& pb_event) override;
    virtual void deactivate() override;
    int first_step_count; // Step count received in first config_event
    int persist_step_count_current; //accumulative steps taken by the user since last activated
    int persist_step_count_update; //accumulative steps taking till now, update per each new step
    virtual void qsh_conn_error_cb(ISession::error e) override;
};

step_count::step_count(suid sensor_uid, sensor_wakeup_type wakeup):
    qsh_sensor(sensor_uid, wakeup), first_step_count(-1), persist_step_count_update(0)
{
    set_type(SENSOR_TYPE_STEP_COUNTER);
    set_string_type(SENSOR_STRING_TYPE_STEP_COUNTER);
    set_fifo_reserved_count(PEDOMETER_RESERVED_FIFO_COUNT);
    set_nowk_msgid(SNS_PEDOMETER_MSGID_SNS_STEP_EVENT_CONFIG);
    if(false == is_resolution_set)
        set_resolution(1.0f);
    if(false == is_max_range_set)
        set_max_range(numeric_limits<uint32_t>::max());
    _qsh_intf_id = qsh_intf_resource_manager::get_instance().register_sensor(get_sensor_info());
}

void step_count::qsh_conn_error_cb(ISession::error e)
{
    sns_logi("handle %d error for step_count", e);
    first_step_count = -1;
    qsh_sensor::qsh_conn_error_cb(e);
}

void step_count::deactivate()
{
    /* base class deactivate */
    qsh_sensor::deactivate();
    first_step_count = -1;
}

void step_count::handle_sns_client_event(
        const sns_client_event_msg_sns_client_event& pb_event)
{
    Event hal_event =
         create_sensor_hal_event(pb_event.timestamp());
    if (SNS_PEDOMETER_MSGID_SNS_STEP_EVENT == pb_event.msg_id()) {
        sns_step_event pb_step_event;
        pb_step_event.ParseFromString(pb_event.payload());

        if(-1 == this->first_step_count)
        {
            sns_loge("step_count_event: Received pedometer step event prior to config event");
        }
        else
        {
            hal_event.u.stepCount = pb_step_event.step_count()
                                         - this->first_step_count
                                         + this->persist_step_count_current;
            this->persist_step_count_update = hal_event.u.stepCount;
            if(true == can_submit_sample(hal_event))
              events.push_back(hal_event);
        }
        sns_logv("step_count_event: ts=%" PRIu64 " count %i",
                 hal_event.timestamp, pb_step_event.step_count());
    } else if (SNS_PEDOMETER_MSGID_SNS_STEP_EVENT_CONFIG == pb_event.msg_id()) {
        sns_step_event_config pb_step_event_config;
        pb_step_event_config.ParseFromString(pb_event.payload());

        // We may receive multiple config events - ignore all but the first
        if(-1 == this->first_step_count){
          this->first_step_count = pb_step_event_config.step_count();
          this->persist_step_count_current = this->persist_step_count_update;
          hal_event.u.stepCount = this->persist_step_count_update;
          if(true == can_submit_sample(hal_event))
            events.push_back(hal_event);
        }
        sns_logv("step_count_event_config: ts=%llu count %i persist_count_current %i",
        static_cast<unsigned long long>(pb_event.timestamp()), pb_step_event_config.step_count(), this->persist_step_count_current);
    }
}

static bool step_count_module_init()
{
    /* register supported sensor types with factory */
    sensor_factory::register_sensor(SENSOR_TYPE_STEP_COUNTER,
        qsh_sensor::get_available_sensors<step_count>);
    sensor_factory::request_datatype(QSH_DATATYPE_PEDOMETER);
    return true;
}

SENSOR_MODULE_INIT(step_count_module_init);


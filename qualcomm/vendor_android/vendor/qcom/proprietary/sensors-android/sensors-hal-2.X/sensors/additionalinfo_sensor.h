/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#pragma once

#include "qsh_sensor.h"



class additionalinfo_sensor : public qsh_sensor {

public:
    additionalinfo_sensor(suid sensor_uid,
                    sensor_wakeup_type wakeup,
                    int handle_parentsensor,
                    int qsh_intf_id);

    /* send gyro_temp additional info to hal */
    void send_additional_info(int64_t timestamp);

private:
    virtual void handle_sns_client_event(
        const sns_client_event_msg_sns_client_event& pb_event) override;
    int _handle_parentsensor;
    float _temp;
};

/* ===================================================================
** Copyright (c) 2021-2022 Qualcomm Technologies, Inc.
** All Rights Reserved.
** Confidential and Proprietary - Qualcomm Technologies, Inc.
**
** FILE: display_active.h
** ================================================================ */
void register_diag_sensor_event_cb(see_salt *psalt, bool display_events);

void display_active(std::string sensor_event, unsigned int sensor_handle,
                           unsigned int client_connect_id);



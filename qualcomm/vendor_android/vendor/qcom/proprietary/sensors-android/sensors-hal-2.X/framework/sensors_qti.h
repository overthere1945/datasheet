/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#pragma once

/* All QTI defined sensor types must start from QTI_SENSOR_TYPE_BASE
   This is to ensure no collission with the Android sensor types
   defined in sensors.h */
#define QTI_SENSOR_TYPE_BASE 33171000

#define QTI_SENSOR_TYPE_HALL_EFFECT             (QTI_SENSOR_TYPE_BASE + 1)
#define QTI_SENSOR_STRING_TYPE_HALL_EFFECT      "qti.sensor.hall_effect"

#define QTI_SENSOR_TYPE_SAR                     (QTI_SENSOR_TYPE_BASE + 2)
#define QTI_SENSOR_STRING_TYPE_SAR              "qti.sensor.sar"

#define QTI_SENSOR_TYPE_ENG                     (QTI_SENSOR_TYPE_BASE + 3)
#define QTI_SENSOR_STRING_TYPE_ENG              "qti.sensor.eng"

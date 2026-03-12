/*
 * Copyright (c) 2021-2022 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#pragma once

#define SNS_RT_PRIORITY 10
/**
 * @brief Struct to represent sensor's unique ID (128-bit)
 */
struct sensor_uid
{
    sensor_uid() : low(0), high(0) {}
    sensor_uid(uint64_t low, uint64_t high): low(low), high(high) {}
    bool operator==(const sensor_uid& rhs) const
    {
        return (low == rhs.low && high == rhs.high);
    }
    uint64_t low, high;
};

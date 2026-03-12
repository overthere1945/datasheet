/*
 * Copyright (c) 2017-2023 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#pragma once

#define ATRACE_TAG ATRACE_TAG_ALWAYS
#include <cutils/trace.h>

static bool sns_trace_enabled = false;

static inline void SNS_TRACE_ENABLE(bool enable)
{
    sns_trace_enabled = enable;
}

#define SNS_TRACE_INT64(name, value) ({\
    if (CC_UNLIKELY(sns_trace_enabled)) { \
        ATRACE_INT64(name, value); \
    } \
})

#define SNS_TRACE_BEGIN(name) ({\
    if (CC_UNLIKELY(sns_trace_enabled)) { \
        ATRACE_BEGIN(name); \
    } \
})

#define SNS_TRACE_END() ({\
    if (CC_UNLIKELY(sns_trace_enabled)) { \
        ATRACE_END(); \
    } \
})

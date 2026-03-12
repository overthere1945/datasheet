#pragma once
/*=============================================================================
  @file sns_async_com_port.h

@copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  ===========================================================================*/

/*=============================================================================
  Include Files
  ===========================================================================*/
#include <stdint.h>
#include "sns_async_com_port.h"
#include "sns_async_com_port_int.h"
#include "sns_data_stream.h"
#include "sns_list.h"
#include "sns_sensor_util.h"
#include "sns_signal.h"
#include "sns_stream_service.h"
#include "sns_time.h"

typedef struct
{
  /* signal handle provided by the signal utility */
  sns_signal_handle *sig_handle;
} sns_async_com_port_state;

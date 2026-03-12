#pragma once
/** ============================================================================
 * @file
 *
 * @brief Contains information about the reserved message ids that are 
 * specifically used by framework.
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * ===========================================================================*/

/*=============================================================================

The table below consists of the list of all reserved framework message Ids
that are already used

0-127   - Request Messages
128-255 - Non-recurrent events (configuration updates, one-time events, etc)
256-511 - Recurrent and/or periodic events (e.g. sensor samples)


MSG-ID           MESSAGE
---------------------------------------------------
  1              SNS_STD_MSGID_SNS_STD_ATTR_REQ
  2              SNS_STD_MSGID_SNS_STD_FLUSH_REQ
  3              SNS_STD_MSGID_SNS_STD_DEBUG_REQ
  4              SNS_TRANSPORT_PPE_MSGID_SNS_TRANSPORT_PPE_CONFIG
  5              --- unused ---
  6              SNS_CLIENT_SENSOR_MSGID_SNS_CLIENT_SENSOR_CONFIG
  7              SNS_CLIENT_SENSOR_MSGID_SNS_CLIENT_SENSOR_ENABLE
  8              SNS_CLIENT_SENSOR_MSGID_SNS_CLIENT_SENSOR_DISABLE
                 --- NOTE: 10 - 20 Are reserved for Client Manager
  10             SNS_CLIENT_MSGID_SNS_CLIENT_DISABLE_REQ
                 --- NOTE: 120-127 Are reserved
  120            SNS_FW_MSGID_SNS_DESTROY_REQ
  121            SNS_BATCH_MSGID_SNS_BATCH_CONFIG
  122            SNS_CLIENT_BATCH_MSGID_SNS_CLIENT_BATCH_CONFIG
                 --- NOTE: 128-255 - Non-recurrent events
  128            SNS_STD_MSGID_SNS_STD_ATTR_EVENT
  129            SNS_STD_MSGID_SNS_STD_FLUSH_EVENT
  130            SNS_STD_MSGID_SNS_STD_ERROR_EVENT
  131            SNS_STD_MSGID_SNS_STD_DEBUG_RESP
  132            SNS_SIM_MSGID_SNS_SIM_CONFIG_EVENT
  133            SNS_STD_MSGID_SNS_STD_RESAMPLER_CONFIG_EVENT
  250            SNS_FW_MSGID_SNS_DESTROY_COMPLETE_EVENT
                 --- NOTE: 256-511 - Recurrent Events
  256            SNS_CLIENT_SENSOR_MSGID_SNS_CLIENT_SENSOR_EVENT
  257            SNS_TRANSPORT_PPE_MSGID_SNS_TRANSPORT_PPE_EVENT
  258            SNS_BATCH_MSGID_SNS_BATCH_COMPLETE_EVENT
  259            SNS_CLIENT_BATCH_MSGID_SNS_CLIENT_BATCH_EVENT
===========================================================================*/
/* Max framework reserved request message ID as commented in sns_client.proto */
#define SNS_MAX_FRAMEWORK_RESERVED_REQ_ID 127
#define SNS_FW_EVENT_START                128
#define SNS_FW_EVENT_END                  511

#define IS_FW_RESERVED_REQUEST(x) (x <= SNS_MAX_FRAMEWORK_RESERVED_REQ_ID)
#define IS_FW_RESERVED_EVENT(x)                                                \
  ((x >= SNS_FW_EVENT_START) && (x <= SNS_FW_EVENT_END))

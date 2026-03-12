#pragma once

/** ===========================================================================
 * @file
 *
 * @brief
 * This file contains the heap size information.
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 * ============================================================================*/

// @brief All memory sizes are in KBytes

// @brief Primary non-island heap size
// heap id : SNS_HEAP_MAIN
#define SNS_MAIN_HEAP_ALLOC                    768
// @brief This non-island memory is used for
//  1. Client manager batching
//  2. Sensor and Instance memory allocation requests by Memory Service
// heap id : SNS_HEAP_BATCH
#define SNS_BATCH_HEAP_ALLOC                   256
// @brief Event service non-island buffer
#define SNS_EVENT_HEAP_ALLOC                   256
// @brief QSH Island  pool
// heap id : SNS_HEAP_ISLAND
#define SNS_ISLAND_HEAP_ALLOC                  50
// @brief SSC Island pool
// heap id : SNS_HEAP_ISLAND_SSC
#define SNS_ISLAND_SSC_HEAP_ALLOC              10
#define SNS_ISLAND_QSHTECH_HEAP_ALLOC          0

#define SNS_CLIENT_BATCH_HEAP_ALLOC            0
#define SNS_ISLAND_BATCH_HEAP_ALLOC            1
// @brief Island event service buffer
#define SNS_FW_EVENT_SERVICE_BUFFER_ALLOC      15
#ifndef SSC_TARGET_X86
#define SNS_PLAYBACK_ISLAND_BUFFER_ALLOC       0
// @brief Online PB: The non-island heap buffer used for storing chunks of
// input file
#define SNS_PLAYBACK_NI_BUFFER_ALLOC           2048
// @brief Maximum size of the island buffer used for allocating memory for diag
// log packets
#define SNS_DIAG_MAX_ISLAND_ALLOC_SIZE         (15*1024)
#else
// @brief Offline PB: The QSHTECH island Pool buffer used for storing chunks of
// input file
#define SNS_PLAYBACK_ISLAND_BUFFER_ALLOC       (1024*10)
// @brief Offline PB: The non-island heap buffer used for storing chunks of input
// file
#define SNS_PLAYBACK_NI_BUFFER_ALLOC           (1024*15)
// @brief Maximum size of the non-island buffer used for allocating memory for
// diag log packets
#define SNS_DIAG_MAX_ISLAND_ALLOC_SIZE         (100*1024)
#endif

#define SNS_PLAYBACK_HEAP_SIZE                 2048

#ifdef QSH_CAMERA_ENABLE
#define QSH_CAMERA_EAI_USAGE_SIZE              3072
#else
#define QSH_CAMERA_EAI_USAGE_SIZE              0
#endif //QSH_CAMERA_ENABLE
// @brief AMSS heap for glibc functions like malloc, calloc
#define SNS_SENSORSPD_AMSSHEAP_SIZE             ((QSH_CAMERA_EAI_USAGE_SIZE > SNS_PLAYBACK_HEAP_SIZE)? QSH_CAMERA_EAI_USAGE_SIZE: SNS_PLAYBACK_HEAP_SIZE)
#define SNS_DIAG_SERVICE_LOW_MEM_THRESHOLD     200
#define SNS_BATCH_SENSOR_MEM_THRESHOLD_BYTES   50*1024
// @brief PRAM portion size that is used by Q6 com port drivers
#ifdef SNS_ENABLE_DAE
#define SNS_Q6_PRAM_HEAP_SIZE_BYTES            1*1024
#else
#define SNS_Q6_PRAM_HEAP_SIZE_BYTES            UINT_MAX
#endif

/* This client name should match with the client name in the pram core driver
 */
#define SNS_PRAM_CLIENT_NAME                   "SENSORS"

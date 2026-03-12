/**============================================================================
  @file sns_memory_service.c

  @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.

  ===========================================================================*/

/*=============================================================================
  Include Files
  ===========================================================================*/

#include <string.h>
#include "sns_fw_memory_service.h"
#include "sns_fw_sensor.h"
#include "sns_memory_service.h"
#include "sns_types.h"

/*=============================================================================
  Macros and constants
  ===========================================================================*/

/*=============================================================================
  Forward Declarations
  ===========================================================================*/
static void *instance_malloc(struct sns_sensor_instance *, size_t);

static void instance_free(struct sns_sensor_instance *, void *);

static void register_island_memory_vote_cb(sns_sensor *const,
                                           sns_island_memory_vote_cb);

static void *instance_malloc_island(struct sns_sensor_instance *, size_t);

static sns_rc get_memory_pool_name(struct sns_sensor *, char *, size_t);

static void *sensor_malloc(struct sns_sensor *sensor, size_t size);

static void sensor_free(struct sns_sensor *sensor, void *ptr);

static void *sensor_malloc_island(struct sns_sensor *sensor, size_t size);

/*=============================================================================
  Static Definitions
  ===========================================================================*/

static sns_fw_memory_service memory_service SNS_SECTION(".data.sns");
static sns_memory_service_api memory_service_api SNS_SECTION(".data.sns") = {
    .struct_len = sizeof(memory_service_api),
    .malloc = &instance_malloc,
    .free = &instance_free,
    .register_island_memory_vote_cb = &register_island_memory_vote_cb,
    .malloc_island = &instance_malloc_island,
    .get_memory_pool_name = &get_memory_pool_name,
    .sensor_malloc = &sensor_malloc,
    .sensor_free = &sensor_free,
    .sensor_malloc_island = &sensor_malloc_island,
};

/*=============================================================================
  Public Function Definitions
  ===========================================================================*/

static void *instance_malloc(struct sns_sensor_instance *instance, size_t size)
{
  UNUSED_VAR(instance);
  UNUSED_VAR(size);

  return NULL;
}

SNS_SECTION(".text.sns")
static void *instance_malloc_island(struct sns_sensor_instance *instance,
                                    size_t size)
{
  UNUSED_VAR(instance);
  UNUSED_VAR(size);

  return NULL;
}

/*----------------------------------------------------------------------------*/
SNS_SECTION(".text.sns")
static void instance_free(struct sns_sensor_instance *instance, void *ptr)
{
  UNUSED_VAR(instance);
  UNUSED_VAR(ptr);
}
/*----------------------------------------------------------------------------*/

static void register_island_memory_vote_cb(sns_sensor *const sensor,
                                           sns_island_memory_vote_cb cb)
{
  UNUSED_VAR(sensor);
  UNUSED_VAR(cb);
}

void sns_memory_service_cleanup(void *ptr)
{
  UNUSED_VAR(ptr);
}
/*----------------------------------------------------------------------------*/
static sns_rc get_memory_pool_name(struct sns_sensor *sensor, char *dst_buffer,
                                   size_t dst_size)
{
  UNUSED_VAR(sensor);
  UNUSED_VAR(dst_buffer);
  UNUSED_VAR(dst_size);
  return SNS_RC_NOT_SUPPORTED;
}
/*----------------------------------------------------------------------------*/
static void *sensor_malloc(struct sns_sensor *sensor, size_t size)
{
  UNUSED_VAR(sensor);
  UNUSED_VAR(size);

  return NULL;
}

/*----------------------------------------------------------------------------*/
SNS_SECTION(".text.sns")
static void *sensor_malloc_island(struct sns_sensor *sensor, size_t size)
{
  UNUSED_VAR(sensor);
  UNUSED_VAR(size);

  return NULL;
}

/*----------------------------------------------------------------------------*/
SNS_SECTION(".text.sns")
static void sensor_free(struct sns_sensor *sensor, void *ptr)
{
  UNUSED_VAR(sensor);
  UNUSED_VAR(ptr);
}

/*----------------------------------------------------------------------------*/

sns_fw_memory_service *sns_memory_service_init(void)
{
  memory_service.service.api = &memory_service_api;
  memory_service.service.service.type = SNS_MEMORY_SERVICE;
  memory_service.service.service.version = SNS_MEMORY_SERVICE_VERSION;

  return &memory_service;
}
/*----------------------------------------------------------------------------*/

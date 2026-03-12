#pragma once
/**
 * @file sns_interrupt_sensor.h
 *
 * @brief The Interrupt virtual Sensor.
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 **/

#include "sns_osa_thread.h"
#include "sns_sensor.h"
#include "sns_suid_util.h"
#include "sns_registry.pb.h"

/* Registry will specify GPIO Numbers for which Inactive Mode should be set
 * using this TAG */
#define IRQ_GPIO_TAG "irq_"

/* Max number of IRQ lines which uses software timestamp on specified
 * platform.*/
/* Worst case limit. Generally we won't reach this point*/
#define MAX_SW_IRQ_LINES 20

/* Registry will specify GPIO Numbers for which Inactive Mode should be set
 * using this TAG */
#define INACTIVE_GPIO_TAG "inact_"

/* Max number of GPIOs which can have Inactive Mode configured.*/
#define MAX_INACTIVE_GPIOs 20

/* Max number of IRQ supported */
#define MAX_NUM_IRQ 31

/*Interrupt sensor config details as specified in registry*/
typedef struct
{
  // List of irq numbers which uses s/w ts
  uint16_t sw_ts_irq_lines[MAX_SW_IRQ_LINES];
  // List of GPIOs which require Inactive Mode configuration
  uint16_t inact_gpio_list[MAX_INACTIVE_GPIOs];
  // Number of of irq lines specified in registry
  uint8_t sw_ts_irq_len;
  // Number of of Inactive GPIOs specified in registry
  uint8_t inact_gpio_count;
} sns_irq_config;

/** Forward Declaration of Sensor API */
extern const sns_sensor_api interrupt_sensor_api;

/** Interrupt Sensor State. */
typedef struct interrupt_state
{
  sns_osa_thread *thread;
  // Used to search for the dependent sensors.
  // Dep sensors:registry.
  SNS_SUID_LOOKUP_DATA(1) suid_lookup_data;

  // Required for Registry stream
  sns_data_stream *registry_stream;

  uint32_t signal_mask;

  sns_irq_config config;
} interrupt_state;

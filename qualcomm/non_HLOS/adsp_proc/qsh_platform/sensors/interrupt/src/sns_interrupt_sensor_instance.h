#pragma once
/**
 * @file sns_interrupt_sensor_instance.h
 *
 * @brief Interrupt Sensor Instance.
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 **/

#include "sns_com_port_priv.h"
#include "sns_data_stream.h"
#include "sns_event_service.h"
#include "sns_interrupt.h"
#include "sns_interrupt.pb.h"
#include "sns_interrupt_sensor.h"
#include "sns_sensor_instance.h"
#include "sns_signal.h"
#include "sns_sync_com_port_service.h"
#include "sns_time.h"
#include <stdint.h>

/** Forward Declaration of Instance API */
extern const sns_sensor_instance_api interrupt_sensor_instance_api;

/**
 * Ping-Pong buffer for interrupt sensor; must be a power of 2.
 */
#define SNS_INTERRUPTS_CIRCULAR_BUFFER_DEPTH (1 << 3)

_Static_assert((SNS_INTERRUPTS_CIRCULAR_BUFFER_DEPTH > 1 &&
                (SNS_INTERRUPTS_CIRCULAR_BUFFER_DEPTH &
                 (SNS_INTERRUPTS_CIRCULAR_BUFFER_DEPTH - 1)) == 0),
               "SNS_INTERRUPTS_CIRCULAR_BUFFER_DEPTH must be a power of 2");

typedef struct
{
  uint32_t events_occured;
  sns_interrupt_ibi_data circular_buffer[SNS_INTERRUPTS_CIRCULAR_BUFFER_DEPTH];
} read_only_outside_IST;

typedef void (*sns_interrupt_event_fptr)(sns_sensor_instance *const this);

/** Interrupt Sensor Instance State. The Interrupt Sensor
 *  uses a Sensor Instance per irq_num.
 *  Example: If Accel, Gyro and Motion Detect sensors use the
 *  same irq_num then a single Sensor instance handles this
 *  interrupt stream. Where as a Mag sensor stream will be
 *  handled by another Sensor Instance. */
typedef struct sns_interrupt_instance_state
{
  sns_interrupt_event_fptr event_fptr;
  sns_interrupt_notify_function_ptr notify_fptr;
  union
  {
    /** Information for the irq_pin handled by this Instance. */
    sns_interrupt_info irq_info;

    /** Information for IBI handled by this instance */
    sns_ibi_info ibi_info;
  };

  union
  {
    /** Current client config of this Instance. */
    sns_interrupt_req irq_client_req;
    sns_ibi_req ibi_client_req;
  };

  sns_sync_com_port_service *scp_service;
  sns_com_port_priv_handle *priv_handle;
  /** Event Service handle. This is acquired during interrupt
   *  registration. */
  sns_event_service *event_service;

  /** Thread unsafe buffer that should only be written to from
   * IST context and is read only everywhere else
   */
  read_only_outside_IST read_only_buffer;
  uint32_t events_processed;

  sns_osa_thread *thread;
  uint8_t index;

  sns_irq_config irq_config;
} sns_interrupt_instance_state;

/**============================================================================
 * @file sns_interrupt_sensor_instance.c
 *
 * @brief The Interrupt virtual Sensor Instance implementation
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * ===========================================================================*/

/*=============================================================================
  Include Files
  ============================================================================*/

/* Standard lib includes: */
#include <stdio.h>

/* Sensor includes */
#include "sns_assert.h"
#include "sns_fw_gpio_service_internal.h"
#include "sns_gpio.h"
#include "sns_interrupt_sensor.h"
#include "sns_interrupt_sensor_instance.h"
#include "sns_island.h"
#include "sns_island_util.h"
#include "sns_list.h"
#include "sns_math_ops.h"
#include "sns_mem_util.h"
#include "sns_memmgr.h"
#include "sns_osa_thread.h"
#include "sns_printf.h"
#include "sns_rc.h"
#include "sns_request.h"
#include "sns_sensor_event.h"
#include "sns_sensor_instance.h"
#include "sns_service_manager.h"
#include "sns_stream_service.h"
#include "sns_time.h"
#include "sns_types.h"

/* Proto buf includes */
#include "pb_decode.h"
#include "pb_encode.h"
#include "sns_pb_util.h"
#include "sns_interrupt.pb.h"
#include "sns_std.pb.h"

/*=============================================================================
  Static Function Definitions
  ============================================================================*/
/**
 * Optimized encoding for sns_interrupt_events
 *
 * @param[i] this        pointer to the sensor instance
 * @param[i] irq_event   the irq event
 * @param[i] timestamp   the timestamp
 *
 * @return sns_rc        SNS_RC_SUCCESS if successful
 *                       anything else if fail
 *
 */
SNS_SECTION(".text.sns")
static sns_rc sns_irq_send_event(sns_sensor_instance *instance,
                                 sns_interrupt_event *irq_event,
                                 sns_time timestamp)
{
  int stream_index = 0;
  sns_rc rv = SNS_RC_SUCCESS;

  // Encode lengths
  size_t encoded_len_optional =
      (irq_event->ibi_data.size +
       2); // size: tag, len (max varint value of 32), data
  size_t encoded_len_required =
      (sizeof(uint32_t) + sizeof(uint64_t) + 2); // size: 2x tags, data
  size_t encoded_len = (irq_event->has_ibi_data)
                           ? (encoded_len_required + encoded_len_optional)
                           : (encoded_len_required);

  // Failing this check means the proto has been updated such that this decoder
  // will fail in decoding the message
  SNS_VERBOSE_ASSERT(encoded_len_required ==
                         (sns_interrupt_event_size - (32 + 2)),
                     "pb defined size does not match decoder expected size");

  // Event field byte arrays
  char *interrupt_num_byte_array = (char *)&irq_event->interrupt_num;
  char *ts_num_byte_array = (char *)&irq_event->timestamp;
  char *ibi_byte_array = (char *)&irq_event->ibi_data.bytes;

  sns_service_manager *manager = instance->cb->get_service_manager(instance);

  sns_event_service *event_service =
      (sns_event_service *)manager->get_service(manager, SNS_EVENT_SERVICE);

  sns_sensor_event *event =
      event_service->api->alloc_event(event_service, instance, encoded_len);

  uint8_t *encoded_event = (uint8_t *)event->event;

  if(event != NULL)
  {
    /* Manual Event Encoding */

    //------------ Encode interrupt_num
    // Tag
    encoded_event[stream_index++] =
        sns_interrupt_event_interrupt_num_tag << 3 | PB_WT_32BIT;

#ifndef ENDIAN_LITTLE
    // Value (Byte Swapped)
    for(int len = 3; len >= 0; len--)
    {
      encoded_event[stream_index++] = interrupt_num_byte_array[len];
    }

#else
    // Value
    for(int len = 0; len < sizeof(uint32_t); len++)
    {
      encoded_event[stream_index++] = interrupt_num_byte_array[len];
    }

#endif

    //----------- Encode timestamp
    // Tag
    encoded_event[stream_index++] =
        sns_interrupt_event_timestamp_tag << 3 | PB_WT_64BIT;

#ifndef ENDIAN_LITTLE
    // Value (Byte Swapped)
    for(int len = 7; len >= 0; len--)
    {
      encoded_event[stream_index++] = ts_num_byte_array[len];
    }

#else
    // Value
    for(int len = 0; len < sizeof(uint64_t); len++)
    {
      encoded_event[stream_index++] = ts_num_byte_array[len];
    }

#endif

    //----------- Encode optional ibi_data
    if(irq_event->has_ibi_data)
    {
      // Tag
      encoded_event[stream_index++] =
          sns_interrupt_event_ibi_data_tag << 3 | PB_WT_STRING;

      // LEN
      encoded_event[stream_index++] = irq_event->ibi_data.size;

      // Value
      for(int len = 0; len < irq_event->ibi_data.size; len++)
      {
        encoded_event[stream_index++] = ibi_byte_array[len];
      }
    }

    event->event_len = encoded_len;
    event->message_id = SNS_INTERRUPT_MSGID_SNS_INTERRUPT_EVENT;
    event->timestamp = timestamp;

    // Send Event
    rv |=
        event_service->api->publish_event(event_service, instance, event, NULL);
  }
  else
  {
    /* Event Allocation Error */
    rv = SNS_RC_FAILED;
    SNS_INST_PRINTF(ERROR, instance, "Unable to allocate event (len %d; id %d)",
                    encoded_len, SNS_INTERRUPT_MSGID_SNS_INTERRUPT_EVENT);

    sns_sensor_event *event =
        event_service->api->alloc_event(event_service, instance, 0);

    if(event == NULL)
    {
      SNS_INST_PRINTF(ERROR, instance,
                      "Unable to allocate event (len %d; id %d)", 0,
                      SNS_INTERRUPT_MSGID_SNS_INTERRUPT_EVENT);
    }
    else
    {
      event->event_len = 0;
      event->message_id = SNS_INTERRUPT_MSGID_SNS_INTERRUPT_EVENT;
      event->timestamp = timestamp;

      // Send Empty Event
      rv |= event_service->api->publish_event(event_service, instance, event,
                                              NULL);
    }
  }
  return rv;
}

/**
 * Generate and send an sns_interrupt_event for an interrupt
 */
SNS_SECTION(".text.sns")
static void gen_irq_events(sns_sensor_instance *const this)
{
  sns_interrupt_instance_state *state =
      (sns_interrupt_instance_state *)this->state->state;

  for(; state->events_processed < state->read_only_buffer.events_occured;
      state->events_processed++)
  {
    sns_interrupt_event irq_event = sns_interrupt_event_init_default;
    uint32_t index =
        state->events_processed & (SNS_INTERRUPTS_CIRCULAR_BUFFER_DEPTH - 1);
    state->irq_info.timestamp =
        state->read_only_buffer.circular_buffer[index].timestamp;
    if(!state->irq_info.is_registered)
    {
      state->irq_info.warning_cnt++;
      SNS_INST_PRINTF(LOW, this, "Unregistered IRQ %d fired count %d",
                      state->irq_info.irq_num, state->irq_info.warning_cnt++);
    }

    // PEND: Publish the timestamp status so that the clients can handle corner
    //       cases instead of the interrupt sensor having to make all of the
    //       hard decisions.
    if(state->read_only_buffer.circular_buffer[index].ts_status !=
       SNS_INTERRUPT_TIMESTAMP_VALID)
    {
      // Overflow happens if the timestamp is updated in the middle of reading
      // it. That could mean a spurious interrupt happened or the interrupts
      // are coming in too fast for us to handle them in a timely manner
      if(state->read_only_buffer.circular_buffer[index].ts_status ==
         SNS_INTERRUPT_TIMESTAMP_VALID_OVERFLOW)
      {
        SNS_INST_PRINTF(HIGH, this, "Timestamp 0x%x%08x overflowed",
                        (uint32_t)(state->irq_info.timestamp >> 32),
                        (uint32_t)(state->irq_info.timestamp));
      }
      else
      {
        // If the timestamp has an invalid status, just print out a warning
        SNS_INST_PRINTF(
            HIGH, this, "Timestamp 0x%x%08x has invalid status 0x%x",
            (uint32_t)(state->irq_info.timestamp >> 32),
            (uint32_t)(state->irq_info.timestamp),
            state->read_only_buffer.circular_buffer[index].ts_status);
      }
    }

    irq_event.interrupt_num = state->irq_info.irq_num;
    irq_event.timestamp = state->irq_info.timestamp;

    if(sns_irq_send_event(this, &irq_event, sns_get_system_time()) !=
       SNS_RC_SUCCESS)
    {
      SNS_INST_PRINTF(ERROR, this, "Failed to send Interrupt event for irq %d",
                      state->irq_info.irq_num);
    }
  }
}

/**
 * After a Physical sensor driver registers the interrupt,
 * Interrupt Sensor Instance registers this call back with
 * uGPIOInt. ISR calls sns_ddf_smgr_notify_irq(). Do minimal
 * operation in this function because it is in IST
 * context.
 *
 * @param param   context provided to uGPIOInt. It is pointer to
 *                the Sensor Instance in this case.
 *
 */
SNS_SECTION(".text.sns")
static void *sns_interrupt_sensor_notify(uint32_t param)
{
  // PEND: In Napali, only 5 interrupts can utilize the HW timestamping
  //       unit at a time. So we will continue to grab the system time
  //       here to support the other interrupts. Once the HW timestamping
  //       unit can support all interrupts at the same time, then we can
  //       remove this.
  sns_time sig_timestamp;
  sns_interrupt_instance_state *state;
  uint32_t index;
  sns_sensor_instance *this = (sns_sensor_instance *)param;
  sns_interrupt_timestamp_status ts_status = SNS_INTERRUPT_TIMESTAMP_VALID;

  state = (sns_interrupt_instance_state *)this->state->state;

  // If this interrupt uses the hardware timestamping unit, then go out and
  // read the timestamp.
  if(state->irq_info.uses_hw_timestamp_unit)
  {
    sns_rc ret;
    sns_time read_hw_timestamp;
    ret = sns_interrupt_read_timestamp(this, state->irq_info.irq_num,
                                       &read_hw_timestamp, &ts_status);
    // If the timestamp was successfully read, then use it. Otherwise keep
    // using the timestamp that was read at the beginning of this function.
    if(SNS_RC_SUCCESS == ret)
    {
      sig_timestamp = read_hw_timestamp;
      //      SNS_INST_PRINTF(HIGH, this, "read_timestamp succeeded, timestamp
      //      %u", (uint32_t)sig_timestamp);
    }
    else
    {
      sig_timestamp = sns_get_system_time();
    }
    //    else
    //    {
    //      SNS_INST_PRINTF(ERROR, this, "read_timestamp failed %u", ret);
    //    }
  }
  else
  {
    sig_timestamp = sns_get_system_time();
  }

  index = state->read_only_buffer.events_occured &
          (SNS_INTERRUPTS_CIRCULAR_BUFFER_DEPTH - 1);
  state->read_only_buffer.circular_buffer[index].timestamp = sig_timestamp;
  state->read_only_buffer.circular_buffer[index].ts_status = ts_status;
  state->read_only_buffer.events_occured += 1;
  sns_osa_thread_sigs_set(state->thread, 1 << state->index);

  return 0;
}

/**
 Some interrupts are registered with this "exit_island" wrapper. This helps to
 avoid checking if the instance pointer is in island on every interrupt.
*/
SNS_SECTION(".text.sns")
static void *sns_interrupt_sensor_notify_exit_island(uint32_t param)
{
  SNS_ISLAND_EXIT();
  sns_interrupt_sensor_notify(param);
  return 0;
}

/**
 * Checks whether current INT line need to set INACTIVE CFG
 *
 * @param[i] state      current instance state
 *
 * @return
 * True if inactive_cfg is to be set for this INT line, otherwise FALSE
 */
static bool is_inactive_cfg_required(sns_interrupt_instance_state *const state)
{
  int irq_line_index = 0;

  for(irq_line_index = 0;
      irq_line_index < state->irq_config.inact_gpio_count &&
      state->irq_config.inact_gpio_count < MAX_INACTIVE_GPIOs;
      irq_line_index++)
  {
    if(state->irq_config.inact_gpio_list[irq_line_index] ==
       state->irq_info.irq_num)
    {
      SNS_PRINTF(HIGH, sns_fw_printf,
                 "INACTIVE Mode CFG enabled for GPIO Pin = %d",
                 state->irq_info.irq_num);
      return true;
    }
  }
  return false;
}

/**
 * Checks whether current INT line need to use hw or sw Timestamp
 *
 * @param[i] state      current instance state
 *
 * @return
 * True if hw_ts is going to use for this INT line, otherwise FALSE
 */
static bool is_hw_ts_required(sns_interrupt_instance_state *const state)
{
  for(int irq_line_index = 0;
      irq_line_index < state->irq_config.sw_ts_irq_len &&
      state->irq_config.sw_ts_irq_len < MAX_SW_IRQ_LINES;
      irq_line_index++)
  {
    if(state->irq_config.sw_ts_irq_lines[irq_line_index] ==
       state->irq_info.irq_num)
    {
      SNS_PRINTF(HIGH, sns_fw_printf, "HW TS disabled for GPIO Pin = %d",
                 state->irq_info.irq_num);
      return false;
    }
  }
  return true;
}

/**
 * Register the interrupt signal with uGPIOInt or GPIOInt.

 * @param[i] this    current instance
 * @param[i] req     IRQ config request
 *
 * @return
 * SNS_RC_INVALID_VALUE - invalid request
 * SNS_RC_SUCCESS
 */
static sns_rc register_interrupt(sns_sensor_instance *const this,
                                 sns_request const *client_request)
{
  sns_interrupt_req const *new_req =
      (sns_interrupt_req *)client_request->request;
  sns_rc err;
  sns_interrupt_instance_state *state =
      (sns_interrupt_instance_state *)this->state->state;
  bool irq_is_enabled = false, set_inactive_cfg = false;
  sns_gpio_interrupt_config config;
  sns_interrupt_gpio_register_param reg_param = {
      .inst = this, .irq_info = &state->irq_info, .func = state->notify_fptr};

  if(state->irq_info.is_registered)
  {
    /* If the irq_pin is registered then proceed only if
       the incoming request is diffrent from current config.*/
    if(0 == sns_memcmp(new_req, &state->irq_client_req, sizeof(*new_req)))
    {
      /* new_req is same as current config. No-op.*/
      return SNS_RC_SUCCESS;
    }
  }

  state->irq_info.irq_num = new_req->interrupt_num;
  state->irq_info.warning_cnt = 0;
  state->irq_info.timestamp = 0;
  state->irq_info.trigger_type = new_req->interrupt_trigger_type;
  state->irq_info.is_chip_pin = new_req->is_chip_pin;
  state->irq_info.uses_hw_timestamp_unit = is_hw_ts_required(state);

  set_inactive_cfg = is_inactive_cfg_required(state);

  config.req = new_req;
  config.enable = true;
  config.set_inactive_cfg = set_inactive_cfg;
  irq_is_enabled = (SNS_RC_SUCCESS == sns_gpio_configure_interrupt(&config));
  if(!irq_is_enabled)
  {
    SNS_PRINTF(ERROR, sns_fw_printf,
               "register_int:: failed to update pin for irq #%u",
               state->irq_info.irq_num);
    return SNS_RC_INVALID_VALUE;
  }

  err = sns_interrupt_gpio_register(&reg_param);
  if(err == SNS_RC_SUCCESS)
  {
    /* Copy the new irq client config in private state of this Instance.*/
    sns_memscpy(&state->irq_client_req, sizeof(state->irq_client_req), new_req,
                sizeof(*new_req));

    if(!state->irq_info.is_registered)
    {
      state->irq_info.is_registered = true;
    }
  }
  else
  {
    sns_gpio_interrupt_config config;
    config.req = new_req;
    config.enable = false;
    config.set_inactive_cfg = false;
    sns_gpio_configure_interrupt(&config);
    SNS_INST_PRINTF(ERROR, this, "Failed to register gpio err = %d pin = %d",
                    err, state->irq_info.irq_num);
    return SNS_RC_INVALID_VALUE;
  }

  return SNS_RC_SUCCESS;
}

/**
 * De-Register the interrupt signal with uGPIOInt or GPIOInt.

 * @param[i] this    current instance
 *
 * @return
 * SNS_RC_INVALID_VALUE - invalid request
 * SNS_RC_SUCCESS
 */
static sns_rc deregister_interrupt(sns_sensor_instance *const this)
{
  sns_rc err;
  sns_interrupt_instance_state *state =
      (sns_interrupt_instance_state *)this->state->state;

  if(state->irq_info.is_registered)
  {
    sns_interrupt_gpio_register_param reg_param = {
        .inst = this, .irq_info = &state->irq_info, .func = state->notify_fptr};
    err = sns_interrupt_gpio_deregister(&reg_param);
    if(err == SNS_RC_SUCCESS)
    {
      sns_gpio_interrupt_config config;
      config.req = &state->irq_client_req;
      config.enable = false;
      config.set_inactive_cfg = false;
      state->irq_info.is_registered = false;
      sns_gpio_configure_interrupt(&config);
    }
    else
    {
      SNS_INST_PRINTF(ERROR, this,
                      "Failed to de-register gpio err = %d pin = %d", err,
                      state->irq_info.irq_num);
      return SNS_RC_INVALID_VALUE;
    }
  }
  return SNS_RC_SUCCESS;
}

/** See sns_sensor_instance_api::notify_event */
SNS_SECTION(".text.sns")
static sns_rc sns_irq_inst_notify_event(sns_sensor_instance *const this)
{
  UNUSED_VAR(this);
  return SNS_RC_SUCCESS;
}

/** See sns_sensor_instance_api::init */
static sns_rc sns_irq_inst_init(sns_sensor_instance *const this,
                                sns_sensor_state const *sensor_state)
{
  sns_rc rc = SNS_RC_SUCCESS;
  interrupt_state *sstate = (interrupt_state *)sensor_state->state;
  sns_interrupt_instance_state *state =
      (sns_interrupt_instance_state *)this->state->state;
  sns_service_manager *service_manager = this->cb->get_service_manager(this);

  state->event_service = (sns_event_service *)service_manager->get_service(
      service_manager, SNS_EVENT_SERVICE);
  state->scp_service =
      (sns_sync_com_port_service *)service_manager->get_service(
          service_manager, SNS_SYNC_COM_PORT_SERVICE);
  state->notify_fptr = !sns_island_is_island_ptr((intptr_t)this)
                           ? sns_interrupt_sensor_notify_exit_island
                           : sns_interrupt_sensor_notify;
  state->thread = sstate->thread;

  sns_memscpy(&state->irq_config, sizeof(state->irq_config), &sstate->config,
              sizeof(sstate->config));

  return rc;
}

/* Non-island interrupt req handling */
static sns_rc __attribute__((noinline))
handle_interrupt_req(sns_sensor_instance *const this,
                     sns_request const *client_request)
{
  sns_interrupt_instance_state *state =
      (sns_interrupt_instance_state *)this->state->state;
  sns_rc ret_val = SNS_RC_SUCCESS;
  state->event_fptr = gen_irq_events;

  if(SNS_RC_SUCCESS == ret_val)
  {
    state->events_processed = 0;
    state->read_only_buffer.events_occured = 0;

    ret_val = register_interrupt(this, client_request);

    if(SNS_RC_SUCCESS != ret_val ||
       !pb_send_event(this, NULL, NULL, sns_get_system_time(),
                      SNS_INTERRUPT_MSGID_SNS_INTERRUPT_REG_EVENT, NULL))
    {
      SNS_INST_PRINTF(ERROR, this, "Failed to register irq %d",
                      state->irq_info.irq_num);
      ret_val = SNS_RC_INVALID_STATE;
    }
  }
  else
  {
    ret_val = SNS_RC_POLICY;
  }
  return ret_val;
}

/**
 * After a Physical sensor driver registers the IBI,
 * Interrupt Sensor Instance registers this call back with
 * I3C. Do minimal operation in this function because it is in
 * I3C context.
 *
 * @param[out] transfer_status   The completion status of the transfer; see
 *                               #i2c_status for actual values.
 * @param[out] transferred       Total number of bytes transferred. If
 *                               transfer_status is I2C_SUCCESS, then this value
 *                               is equal to the sum of lengths of all the
 *                               descriptors passed in a call to i2c_transfer().
 *                               If transfer_status is not I2C_SUCCESS, this
 *                               value is less than the sum of lengths of all
 * the descriptors passed in a call to i2c_transfer().
 * @param[out] ctxt              The callback context parameter that was passed
 *                               in the call to i2c_transfer().
 *
 */
SNS_SECTION(".text.sns")
static void sns_ibi_handle_sensor_notify(sns_sensor_instance *const this,
                                         sns_interrupt_ibi_data *cb_data)
{
  sns_interrupt_instance_state *state =
      (sns_interrupt_instance_state *)this->state->state;
  uint32_t index;

  index = state->read_only_buffer.events_occured &
          (SNS_INTERRUPTS_CIRCULAR_BUFFER_DEPTH - 1);
  state->read_only_buffer.circular_buffer[index].timestamp = cb_data->timestamp;
  state->read_only_buffer.circular_buffer[index].num_ibi_bytes =
      cb_data->num_ibi_bytes;
  state->read_only_buffer.events_occured += 1;
  sns_osa_thread_sigs_set(state->thread, 1 << state->index);
}

/**
 * Generate and send an sns_interrupt_event for an IBI
 */
SNS_SECTION(".text.sns")
static void gen_ibi_events(sns_sensor_instance *const this)
{
  sns_interrupt_instance_state *state =
      (sns_interrupt_instance_state *)this->state->state;

  if(state->events_processed + 1 < state->read_only_buffer.events_occured)
  {
    SNS_INST_PRINTF(ERROR, this, "Warning: dropped IBI data");
  }

  for(; state->events_processed < state->read_only_buffer.events_occured;
      state->events_processed++)
  {
    sns_interrupt_event irq_event = sns_interrupt_event_init_default;
    uint32_t index =
        state->events_processed & (SNS_INTERRUPTS_CIRCULAR_BUFFER_DEPTH - 1);

    state->ibi_info.timestamp =
        state->read_only_buffer.circular_buffer[index].timestamp;
    if(NULL == state->ibi_info.pal_config)
    {
      SNS_INST_PRINTF(ERROR, this, "Unregistered IBI fired");
    }

    irq_event.interrupt_num = state->ibi_client_req.dynamic_slave_addr;
    irq_event.timestamp = state->ibi_info.timestamp;
    if(state->read_only_buffer.circular_buffer[index].num_ibi_bytes > 0)
    {
      irq_event.ibi_data.size =
          state->read_only_buffer.circular_buffer[index].num_ibi_bytes;
      irq_event.has_ibi_data = true;
      sns_memscpy(irq_event.ibi_data.bytes, sizeof(irq_event.ibi_data.bytes),
                  state->ibi_info.ibi_data_buf,
                  sizeof(state->ibi_info.ibi_data_buf));
    }
    if(SNS_RC_SUCCESS !=
       sns_irq_send_event(this, &irq_event, sns_get_system_time()))
    {
      SNS_INST_PRINTF(
          ERROR, this,
          "Failed to send Interrupt event for ibi slave:%d instance:%d",
          state->ibi_client_req.dynamic_slave_addr,
          state->ibi_client_req.bus_instance);
    }
    // todo: remove debug
    else
    {
      SNS_PRINTF(MED, sns_fw_printf, "sent ibi event ibi_data.size: %d",
                 irq_event.ibi_data.size);
    }
  }
}

SNS_SECTION(".text.sns")
static sns_rc __attribute__((noinline))
handle_ibi_req(sns_sensor_instance *const this,
               sns_request const *client_request)
{
  sns_rc ret_val = SNS_RC_SUCCESS;
  sns_interrupt_instance_state *state =
      (sns_interrupt_instance_state *)this->state->state;
  sns_ibi_req const *new_req = (sns_ibi_req *)client_request->request;

  state->event_fptr = gen_ibi_events;
  state->ibi_info.notify = &sns_ibi_handle_sensor_notify;
  state->ibi_info.inst = this;

  ret_val = sns_interrupt_register_ibi(this, new_req, &state->ibi_client_req,
                                       &state->ibi_info);

  if(SNS_RC_SUCCESS != ret_val ||
     !pb_send_event(this, NULL, NULL, sns_get_system_time(),
                    SNS_INTERRUPT_MSGID_SNS_INTERRUPT_REG_EVENT, NULL))
  {
    SNS_INST_PRINTF(ERROR, this, "Failed to register ibi");
    ret_val = SNS_RC_INVALID_STATE;
    (void)sns_interrupt_deregister_ibi(this, &state->ibi_client_req,
                                       &state->ibi_info);
  }
  return ret_val;
}

/** See sns_sensor_instance_api::set_client_config */
SNS_SECTION(".text.sns")
static sns_rc sns_irq_inst_set_client_config(sns_sensor_instance *const this,
                                             sns_request const *client_request)
{
  sns_rc ret_val = SNS_RC_SUCCESS;
  sns_interrupt_instance_state *state =
      (sns_interrupt_instance_state *)this->state->state;

  if(client_request->message_id == SNS_INTERRUPT_MSGID_SNS_INTERRUPT_REQ)
  {
    SNS_ISLAND_EXIT();
    ret_val = handle_interrupt_req(this, client_request);
  }
  else if(client_request->message_id == SNS_INTERRUPT_MSGID_SNS_IBI_REQ)
  {
    ret_val = handle_ibi_req(this, client_request);
  }
  else if(client_request->message_id ==
          SNS_INTERRUPT_MSGID_SNS_INTERRUPT_IS_CLEARED)
  {
    if(state->irq_info.is_registered)
    {
      sns_interrupt_reenable(this, state->irq_info.irq_num);
    }
    else
    {
      SNS_INST_PRINTF(ERROR, this, "Irq number %u not registered",
                      state->irq_info.irq_num);
    }
  }
  else
  {
    SNS_INST_PRINTF(ERROR, this, "Unsupported message ID %d",
                    client_request->message_id);
  }
  return ret_val;
}

static sns_rc sns_irq_inst_deinit(sns_sensor_instance *const this)
{
  sns_interrupt_instance_state *state =
      (sns_interrupt_instance_state *)this->state->state;

  if(gen_irq_events == state->event_fptr)
  {
    deregister_interrupt(this);
  }
  else if(gen_ibi_events == state->event_fptr)
  {
    sns_interrupt_deregister_ibi(this, &state->ibi_client_req,
                                 &state->ibi_info);
  }

  return SNS_RC_SUCCESS;
}

/** Public Data Definitions. */

const sns_sensor_instance_api interrupt_sensor_instance_api SNS_SECTION(
    ".rodata.sns") = {.struct_len = sizeof(sns_sensor_instance_api),
                      .init = &sns_irq_inst_init,
                      .deinit = &sns_irq_inst_deinit,
                      .set_client_config = &sns_irq_inst_set_client_config,
                      .notify_event = &sns_irq_inst_notify_event};

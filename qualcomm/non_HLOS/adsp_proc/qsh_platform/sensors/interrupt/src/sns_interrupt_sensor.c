/**
 * @file sns_interrupt_sensor.c
 *
 * @brief The Interrupt virtual Sensor implementation
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 **/

#include "sns_attribute_service.h"
#include "sns_attribute_util.h"
#include "sns_event_service.h"
#include "sns_fw_event_service.h"
#include "sns_fw_sensor.h"
#include "sns_interrupt_sensor.h"
#include "sns_interrupt_sensor_instance.h"
#include "sns_island.h"
#include "sns_island_util.h"
#include "sns_mem_util.h"
#include "sns_printf.h"
#include "sns_sensor.h"
#include "sns_sensor_instance.h"
#include "sns_sensor_uid.h"
#include "sns_sensor_util.h"
#include "sns_service.h"
#include "sns_service_manager.h"
#include "sns_stream_service.h"
#include "sns_thread_table.h"
#include "sns_thread_utilization.h"
#include "sns_types.h"

#include "pb_decode.h"
#include "pb_encode.h"
#include "sns_pb_util.h"
#include "sns_std.pb.h"
#include "sns_suid.pb.h"

/*=============================================================================
  Macros and Constants
  ===========================================================================*/

/**
 * Sensor UID
 */
static const sns_sensor_uid sensor_uid SNS_SECTION(".rodata.sns") = {
    .sensor_uid = {0x61, 0x8f, 0xf5, 0xe2, 0x56, 0x11, 0x46, 0xe2, 0xb6, 0xcf,
                   0xe3, 0x6e, 0xee, 0x3d, 0xd0, 0x45}};

/**
 * Initialize attributes to their default state.  They may/will be updated
 * within notify_event.
 */
static void publish_attributes(sns_sensor *const this)
{
  {
    char const name[] = "interrupt";
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.str.funcs.encode = pb_encode_string_cb;
    value.str.arg = &((pb_buffer_arg){.buf = name, .buf_len = sizeof(name)});
    sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_NAME, &value, 1, false);
  }
  {
    char const type[] = "interrupt";
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.str.funcs.encode = pb_encode_string_cb;
    value.str.arg = &((pb_buffer_arg){.buf = type, .buf_len = sizeof(type)});
    sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_TYPE, &value, 1, false);
  }
  {
    char const vendor[] = "QTI_internal";
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.str.funcs.encode = pb_encode_string_cb;
    value.str.arg =
        &((pb_buffer_arg){.buf = vendor, .buf_len = sizeof(vendor)});
    sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_VENDOR, &value, 1, false);
  }
  {
    char const proto_files[] = "sns_interrupt.proto";
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.str.funcs.encode = pb_encode_string_cb;
    value.str.arg =
        &((pb_buffer_arg){.buf = proto_files, .buf_len = sizeof(proto_files)});
    sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_API, &value, 1, false);
  }
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_sint = true;
    value.sint = 1;
    sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_VERSION, &value, 1, true);
  }
}

/**
 * Publish the availabilty of this sensor
 */
static void publish_available(sns_sensor *const this, bool available)
{
  sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
  value.has_boolean = true;
  value.boolean = available;
  sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_AVAILABLE, &value, 1, true);
}

/**
 * Parse irq lin numbers from registry items and store the same in config array.
 */
static bool sns_irq_parse_registry(pb_istream_t *stream,
                                   const pb_field_t *field, void **arg)
{
  UNUSED_VAR(field);
  sns_registry_data_item reg_item;
  pb_buffer_arg item_name = {0, 0};
  reg_item.name.arg = &item_name;
  reg_item.name.funcs.decode = pb_decode_string_cb;
  bool rv = pb_decode_noinit(stream, sns_registry_data_item_fields, &reg_item);
  sns_irq_config *config = (sns_irq_config *)*arg;

  if((char *)item_name.buf != NULL)
  {
    if(strncmp((char *)item_name.buf, INACTIVE_GPIO_TAG,
               strlen(INACTIVE_GPIO_TAG)) == 0)
    {
      if(config->inact_gpio_count < MAX_INACTIVE_GPIOs && reg_item.has_sint)
      {
        config->inact_gpio_list[config->inact_gpio_count] =
            (uint16_t)reg_item.sint;
        SNS_PRINTF(LOW, sns_fw_printf, "INACT GPIO #%d = %d",
                   config->inact_gpio_count,
                   config->inact_gpio_list[config->inact_gpio_count]);
        config->inact_gpio_count++;
      }
      else
      {
        SNS_PRINTF(ERROR, sns_fw_printf,
                   "sns_irq_parse_registry Error in parsing lp_gpio");
      }
    }
    else if(strncmp((char *)item_name.buf, IRQ_GPIO_TAG,
                    strlen(IRQ_GPIO_TAG)) == 0)
    {
      if(config->sw_ts_irq_len < MAX_SW_IRQ_LINES && reg_item.has_sint)
      {
        config->sw_ts_irq_lines[config->sw_ts_irq_len] =
            (uint16_t)reg_item.sint;
        SNS_PRINTF(LOW, sns_fw_printf, "IRQ GPIO #%d = %d",
                   config->sw_ts_irq_len,
                   config->sw_ts_irq_lines[config->sw_ts_irq_len]);
        config->sw_ts_irq_len++;
      }
      else
      {
        SNS_PRINTF(ERROR, sns_fw_printf,
                   "sns_irq_parse_registry Error in parsing irq_gpio");
      }
    }
    else
    {
      SNS_PRINTF(ERROR, sns_fw_printf,
                 "sns_irq_parse_registry Error in parsing reg_item");
    }
  }

  return rv;
}

/*Process sns_irq registry event */
static sns_rc handle_irq_registry_event(sns_sensor *const this)
{
  sns_rc rv = SNS_RC_SUCCESS;
  interrupt_state *state = (interrupt_state *)this->state->state;
  sns_data_stream *stream = state->registry_stream;
  bool remove_stream = false;
  if(NULL == stream || 0 == stream->api->get_input_cnt(stream))
  {
    return rv;
  }

  for(sns_sensor_event *event = stream->api->peek_input(stream); NULL != event;
      event = stream->api->get_next_input(stream))
  {

    if(SNS_REGISTRY_MSGID_SNS_REGISTRY_READ_EVENT == event->message_id)
    {
      pb_istream_t stream =
          pb_istream_from_buffer((void *)event->event, event->event_len);
      sns_registry_read_event read_event = sns_registry_read_event_init_default;
      pb_buffer_arg group_name = {0, 0};
      read_event.name.arg = &group_name;
      read_event.name.funcs.decode = pb_decode_string_cb;
      read_event.data.items.arg = &state->config;
      read_event.data.items.funcs.decode = sns_irq_parse_registry;

      if(!pb_decode_noinit(&stream, sns_registry_read_event_fields,
                           &read_event))
      {
        SNS_PRINTF(ERROR, this, "Error decoding registry event");
      }
      else if(0 == strcmp((char *)group_name.buf, "sns_irq"))
      {
        publish_available(this, true);
        remove_stream = true;
      }
      else
      {
        SNS_PRINTF(HIGH, this, "Interrupt config failed");
      }
    }
    else if(SNS_STD_MSGID_SNS_STD_ERROR_EVENT == event->message_id)
    {
      SNS_PRINTF(ERROR, this, "Error event received %%d", event->message_id);
      break;
    }
    else
    {
      SNS_PRINTF(ERROR, this, "Unknown event received %d", event->message_id);
    }
  }
  if(remove_stream)
  {
    sns_sensor_util_remove_sensor_stream(this, &state->registry_stream);
  }

  return rv;
}

/*send registry request with sns_irq */
static void sns_irq_registry_req(sns_sensor *const this)
{
  interrupt_state *state = (interrupt_state *)this->state->state;
  sns_service_manager *service_mgr = this->cb->get_service_manager(this);
  sns_stream_service *stream_service =
      (sns_stream_service *)service_mgr->get_service(service_mgr,
                                                     SNS_STREAM_SERVICE);
  uint8_t buffer[100];
  size_t encoded_len;
  sns_sensor_uid registry_suid;
  sns_suid_lookup_get(&state->suid_lookup_data, "registry", &registry_suid);
  // Create a data stream to Registry
  stream_service->api->create_sensor_stream(stream_service, this, registry_suid,
                                            &state->registry_stream);
  // Send a request to Registry
  sns_registry_read_req sns_interrupt_registry;
  pb_buffer_arg data =
      (pb_buffer_arg){.buf = "sns_irq", .buf_len = strlen("sns_irq") + 1};
  sns_interrupt_registry.name.arg = &data;
  sns_interrupt_registry.name.funcs.encode = pb_encode_string_cb;
  sns_std_request registry_std_req = sns_std_request_init_default;
  encoded_len =
      pb_encode_request(buffer, sizeof(buffer), &sns_interrupt_registry,
                        sns_registry_read_req_fields, &registry_std_req);
  if(0 < encoded_len && NULL != state->registry_stream)
  {
    sns_request request =
        (sns_request){.message_id = SNS_REGISTRY_MSGID_SNS_REGISTRY_READ_REQ,
                      .request_len = encoded_len,
                      .request = buffer};
    state->registry_stream->api->send_request(state->registry_stream, &request);
  }
}

/* See sns_sensor::notify_event */
static sns_rc sns_irq_notify_event(sns_sensor *const this)
{
  interrupt_state *state = (interrupt_state *)this->state->state;
  bool required_complete = sns_suid_lookup_complete(&state->suid_lookup_data);

  if(NULL != state->registry_stream)
  {
    handle_irq_registry_event(this);
  }

  sns_suid_lookup_handle(this, &state->suid_lookup_data);

  if(required_complete != sns_suid_lookup_complete(&state->suid_lookup_data))
  {
    sns_irq_registry_req(this);
    sns_suid_lookup_deinit(this, &state->suid_lookup_data);
  }

  return SNS_RC_SUCCESS;
}

SNS_SECTION(".text.sns") static void irq_task(void *args)
{
  sns_sensor *this = (sns_sensor *)args;
  interrupt_state *state = (interrupt_state *)this->state->state;
  sns_osa_thread_sigs sigs;
  struct sns_watchdog_voter *wd_voter =
      sns_osa_thread_get_wd_voter(state->thread);

  sns_osa_thread_active(wd_voter);
  sns_thread_utilization_compute(state->thread, START_COMPUTE);

  for(;;)
  {
    sigs = 0;
    sns_thread_utilization_compute(state->thread, STOP_COMPUTE);

    sns_osa_thread_sigs_wait_wrapper(0xFFFFFFFF, &sigs, state->thread);
    sns_es_low_mem_block();
    sns_thread_utilization_compute(state->thread, START_COMPUTE);

    if(sigs & SNS_OSA_THREAD_EXIT_SIG)
    {
      SNS_PRINTF(HIGH, this, "Thread Exit");
      break;
    }
    else
    {
      sns_osa_lock_acquire(((sns_fw_sensor *)this)->library->library_lock);

      sns_sensor_instance *inst;
      for(inst = this->cb->get_sensor_instance(this, true); NULL != inst;
          inst = this->cb->get_sensor_instance(this, false))
      {
        sns_interrupt_instance_state *inst_state =
            (sns_interrupt_instance_state *)inst->state->state;
        if(sigs & 1 << inst_state->index)
        {
          inst_state->event_fptr(inst);
        }
      }

      sns_osa_lock_release(((sns_fw_sensor *)this)->library->library_lock);
    }
  }
}

/* See sns_sensor::init */
static sns_rc sns_irq_init(sns_sensor *const this)
{
  sns_rc rc = SNS_RC_FAILED;
  interrupt_state *state = (interrupt_state *)this->state->state;

  rc = sns_osa_thread_create_v2(SNS_IRQ_THREAD, (void *)&irq_task, this,
                                &state->thread);
  if(SNS_RC_SUCCESS == rc)
  {
    publish_attributes(this);

    state->config.sw_ts_irq_len = 0;
    state->config.inact_gpio_count = 0;

#ifndef SNS_DISABLE_REGISTRY
    SNS_SUID_LOOKUP_INIT(state->suid_lookup_data, NULL);
    sns_suid_lookup_add_v2(this, &state->suid_lookup_data, "registry", true);
#else
    publish_available(this, true);
#endif
  }
  return rc;
}

/* See sns_sensor::get_sensor_uid */
SNS_SECTION(".text.sns")
static sns_sensor_uid const *
sns_irq_get_sensor_uid(sns_sensor const *const this)
{
  UNUSED_VAR(this);
  return &sensor_uid;
}

/**
 * See sns_sensor_util_instance_match_cb
 */
SNS_SECTION(".text.sns")
static bool find_instance_match(struct sns_request const *request,
                                sns_sensor_instance const *instance)
{
  sns_interrupt_instance_state const *inst_state =
      (sns_interrupt_instance_state *)instance->state->state;
  bool rv = false;

  /*Use only irq_num in request to find an instance match.
    The idea is to have one instance per irq_num.
    Say accel is streaming and had already registered the interupt
    and then gyro wants to change drive strength of the pin,
    we should be able to change config of the same instance and not
    create a new one. In case both accel and gyro have the same irq
    config then the instance will handle a no-op like operation.*/
  if(SNS_INTERRUPT_MSGID_SNS_INTERRUPT_REQ == request->message_id)
  {
    rv = (inst_state->irq_client_req.interrupt_num ==
          ((sns_interrupt_req *)request->request)->interrupt_num);
  }
  else if(SNS_INTERRUPT_MSGID_SNS_IBI_REQ == request->message_id)
  {
    rv = (inst_state->ibi_client_req.dynamic_slave_addr ==
          ((sns_ibi_req *)request->request)->dynamic_slave_addr) &&
         (inst_state->ibi_client_req.bus_instance ==
          ((sns_ibi_req *)request->request)->bus_instance);
  }
  return rv;
}

/* See sns_sensor::set_client_request */
SNS_SECTION(".text.sns")
static sns_sensor_instance *
sns_irq_set_client_request(sns_sensor *const this,
                           struct sns_request const *exist_request,
                           struct sns_request const *new_request, bool remove)
{
  sns_sensor_instance *curr_inst = sns_sensor_util_find_instance(
      this, exist_request, this->sensor_api->get_sensor_uid(this));
  sns_sensor_instance *rv_inst = NULL;

  if(remove)
  {
    if(NULL != curr_inst)
    {
      curr_inst->cb->remove_client_request(curr_inst, exist_request);
    }
  }
  else
  {
    pb_istream_t stream;
    sns_request decoded_new_request;
    union
    {
      sns_interrupt_req int_req_payload;
      sns_ibi_req ibi_req_payload;
    } payload;
    sns_std_request request = sns_std_request_init_default;
    pb_simple_cb_arg int_arg = {.decoded_struct = &payload,
                                .fields = sns_interrupt_req_fields};
    pb_simple_cb_arg ibi_arg = {.decoded_struct = &payload,
                                .fields = sns_ibi_req_fields};

    if(new_request->message_id != SNS_INTERRUPT_MSGID_SNS_INTERRUPT_REQ &&
       new_request->message_id !=
           SNS_INTERRUPT_MSGID_SNS_INTERRUPT_IS_CLEARED &&
       new_request->message_id != SNS_INTERRUPT_MSGID_SNS_IBI_REQ)
    {
      SNS_PRINTF(ERROR, this, "Unsupported message id = %d",
                 new_request->message_id);
      return NULL;
    }

    if(NULL == exist_request)
    {
      /** new request for interrupt sensor */
      stream = pb_istream_from_buffer(new_request->request,
                                      new_request->request_len);
    }
    else
    {
      /** updated request/message to/for interrupt sensor
       *  Continue using the existing request to extract interrupt num.
       *  This should be the original interrupt registration request. */
      stream = pb_istream_from_buffer(exist_request->request,
                                      exist_request->request_len);
    }

    if(new_request->message_id == SNS_INTERRUPT_MSGID_SNS_INTERRUPT_REQ)
    {
      request.payload = (struct pb_callback_s){
          .funcs.decode = &pb_decode_simple_cb, .arg = &int_arg};
    }
    else if(new_request->message_id == SNS_INTERRUPT_MSGID_SNS_IBI_REQ)
    {
      request.payload = (struct pb_callback_s){
          .funcs.decode = &pb_decode_simple_cb, .arg = &ibi_arg};
    }

    if(pb_decode_noinit(&stream, sns_std_request_fields, &request))
    {
      decoded_new_request.message_id = new_request->message_id;
      decoded_new_request.request_len = sizeof(payload);
      decoded_new_request.request = &payload;

      sns_sensor_instance *match_inst = sns_sensor_util_find_instance_match(
          this, &decoded_new_request, &find_instance_match);

      if(NULL == curr_inst)
      {
        /** If this is a request from a new client */
        if(NULL == match_inst &&
           (new_request->message_id == SNS_INTERRUPT_MSGID_SNS_INTERRUPT_REQ ||
            new_request->message_id == SNS_INTERRUPT_MSGID_SNS_IBI_REQ))
        {
          interrupt_state *state = (interrupt_state *)this->state->state;
          uint32_t signals = state->signal_mask;
          uint32_t mask = 1;
          for(int i = 0; i < MAX_NUM_IRQ; i++)
          {
            if(!(signals & mask))
            {
              rv_inst = this->cb->create_instance(
                  this, sizeof(sns_interrupt_instance_state));

              sns_interrupt_instance_state *inst_state =
                  (sns_interrupt_instance_state *)rv_inst->state->state;

              state->signal_mask |= mask;
              inst_state->index = i;

              break;
            }
            mask = mask << 1;
          }
        }
        else
        {
          rv_inst = match_inst;
        }
      }
      else
      {
        if(NULL != match_inst)
        {
          rv_inst = match_inst;
        }
        else if(NULL != curr_inst->cb->get_client_request(
                            curr_inst, sns_irq_get_sensor_uid(this), true))
        {
          rv_inst = curr_inst;
        }
        else
        {
          if(new_request->message_id == SNS_INTERRUPT_MSGID_SNS_INTERRUPT_REQ ||
             new_request->message_id == SNS_INTERRUPT_MSGID_SNS_IBI_REQ)
          {
            interrupt_state *state = (interrupt_state *)this->state->state;
            uint32_t signals = state->signal_mask;
            uint32_t mask = 1;
            for(int i = 0; i < MAX_NUM_IRQ; i++)
            {
              if(!(signals & mask))
              {
                rv_inst = this->cb->create_instance(
                    this, sizeof(sns_interrupt_instance_state));

                sns_interrupt_instance_state *inst_state =
                    (sns_interrupt_instance_state *)rv_inst->state->state;
                state->signal_mask |= mask;
                inst_state->index = i;

                break;
              }
              mask = mask << 1;
            }
          }
          else
          {
            SNS_PRINTF(ERROR, this, "Cannot create instance for msg id = %d",
                       new_request->message_id);
          }
        }
      }
    }

    if((NULL != curr_inst) &&
       ((new_request->message_id == SNS_INTERRUPT_MSGID_SNS_INTERRUPT_REQ) ||
        (new_request->message_id == SNS_INTERRUPT_MSGID_SNS_IBI_REQ)))
    {
      curr_inst->cb->remove_client_request(curr_inst, exist_request);
    }

    if(NULL != rv_inst)
    {
      /** Interrupt sensor only adds a new interrupt registration request
       *  to the instance's list of client requests.
       *  Interrupt is cleared messages are handled as transitory messages. */
      if((new_request->message_id == SNS_INTERRUPT_MSGID_SNS_INTERRUPT_REQ) ||
         (new_request->message_id == SNS_INTERRUPT_MSGID_SNS_IBI_REQ))
      {
        rv_inst->cb->add_client_request(rv_inst, new_request);
      }

      this->instance_api->set_client_config(rv_inst, &decoded_new_request);
    }
  }

  if(NULL != curr_inst &&
     NULL == curr_inst->cb->get_client_request(
                 curr_inst, sns_irq_get_sensor_uid(this), true))
  {
    interrupt_state *state = (interrupt_state *)this->state->state;
    sns_interrupt_instance_state *inst_state =
        (sns_interrupt_instance_state *)curr_inst->state->state;
    state->signal_mask &= ~(1 << inst_state->index);
    this->cb->remove_instance(curr_inst);
  }

  return rv_inst;
}

static sns_rc sns_irq_deinit(sns_sensor *const this)
{
  interrupt_state *state = (interrupt_state *)this->state->state;
  sns_rc rc = SNS_RC_SUCCESS;

  sns_osa_thread_sigs_set(state->thread, SNS_OSA_THREAD_EXIT_SIG);
  if(SNS_RC_SUCCESS != sns_osa_thread_delete(state->thread))
  {
    rc = SNS_RC_INVALID_STATE;
  }
  sns_suid_lookup_deinit(this, &state->suid_lookup_data);
  return rc;
}

/** Public Data Definitions. */

const sns_sensor_api interrupt_sensor_api SNS_SECTION(".rodata.sns") = {
    .struct_len = sizeof(sns_sensor_api),
    .init = &sns_irq_init,
    .deinit = &sns_irq_deinit,
    .get_sensor_uid = &sns_irq_get_sensor_uid,
    .set_client_request = &sns_irq_set_client_request,
    .notify_event = &sns_irq_notify_event,
};

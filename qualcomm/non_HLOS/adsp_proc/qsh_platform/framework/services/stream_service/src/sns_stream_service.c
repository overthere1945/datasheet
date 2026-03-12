/**============================================================================
  @file

  @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.

  ===========================================================================*/

/*=============================================================================
  Include Files
  ===========================================================================*/
#include <stdlib.h>
#include "sns_assert.h"
#include "sns_fw_basic_services.h"
#include "sns_fw.pb.h"
#include "sns_fw_attribute_service.h"
#include "sns_fw_data_stream.h"
#include "sns_fw_event_service.h"
#include "sns_fw_log.h"
#include "sns_fw_request.h"
#include "sns_fw_sensor.h"
#include "sns_fw_sensor_event.h"
#include "sns_fw_sensor_instance.h"
#include "sns_island.h"
#include "sns_island_util.h"
#include "sns_mem_util.h"
#include "sns_memmgr.h"
#include "sns_printf_int.h"
#include "sns_router.h"
#include "sns_sensor_instance.h"
#include "sns_sensor_util.h"
#include "sns_service_manager.h"
#include "sns_std.pb.h"
#include "sns_thread_manager.h"
#include "sns_types.h"

/*=============================================================================
  Macros and constants
  ===========================================================================*/
// Length of the largest request message that will be stored within island
// memory
#define SNS_STREAM_MAX_ISLAND_REQ_LEN 150

/*=============================================================================
  Type Definitions
  ===========================================================================*/

/**
 * Some messages will be handled specially by the Framework; these messages
 * are stored in the msg_handlers table below, using this format.
 */
typedef struct sns_msg_handler
{
  uint32_t msg_id;
  sns_rc (*handle_req)(sns_sensor *, sns_fw_request *);
} sns_msg_handler;

/*=============================================================================
  Forwared Declarations
  ===========================================================================*/
static sns_rc create_sensor_stream(sns_stream_service *, sns_sensor *,
                                   sns_sensor_uid, struct sns_data_stream **);
static sns_rc create_sensor_instance_stream(sns_stream_service *,
                                            sns_sensor_instance *,
                                            sns_sensor_uid,
                                            struct sns_data_stream **);
static sns_rc remove_stream(sns_stream_service *, struct sns_data_stream *);

/*=============================================================================
  Static Data Definitions
  ===========================================================================*/

static sns_fw_stream_service stream_service SNS_SECTION(".data.sns");
static const uint16_t version SNS_SECTION(".rodata.sns") = 1;
static const sns_stream_service_api
    stream_service_api SNS_SECTION(".rodata.sns") = {
        .struct_len = sizeof(stream_service_api),
        .create_sensor_stream = &create_sensor_stream,
        .create_sensor_instance_stream = &create_sensor_instance_stream,
        .remove_stream = &remove_stream};

/*=============================================================================
  Public Data Definitions
  ===========================================================================*/

/* Declared and commented in sns_sensor.h */
sns_sensor_instance sns_instance_no_error SNS_SECTION(".data.sns");

/*=============================================================================
  Static Function Definitions
  ===========================================================================*/

/**
 * Add integrity checksum to a request
 *
 * @param[i] request Request to which checksum will be added
 */
SNS_SECTION(".text.sns")
static void request_add_checksum(sns_fw_request *request)
{
  if(NULL != request)
  {
    request->sanity =
        (SNS_STREAM_SERVICE_REQUEST_SANITY ^
         (uint32_t)request->request.request_len ^ (uint32_t)(uintptr_t)request);
  }
}

/**
 * Determine whether the given sensor matches the search query.
 *
 * @return false to stop search; true to continue
 */
static bool __attribute__((noinline))
sns_find_sensor(struct sns_attribute_info *attr_info, void *arg)
{
  sns_find_sensor_arg *find_arg = (sns_find_sensor_arg *)arg;
  sns_sensor_uid suid = sns_attr_info_get_suid(attr_info);

  if(sns_sensor_uid_compare(find_arg->suid, &suid))
  {
    find_arg->sensor = sns_attr_info_get_sensor(attr_info);
    find_arg->priority = sns_attr_info_get_priority(attr_info);
    find_arg->available = sns_attr_info_get_available(attr_info);
    sns_attr_info_get_data_type(attr_info, find_arg->data_type,
                                sizeof(find_arg->data_type));
    find_arg->island_req = sns_attr_info_get_island_req(attr_info);
    find_arg->library = sns_attr_info_get_library(attr_info);
    return false;
  }

  return true;
}


/**
 * Determine whether the given remote sensor matches the search query.
 *
 * @return false to stop search; true to continue
 */
static bool __attribute__((noinline))
sns_find_remote_sensor(struct sns_remote_sensor *remote_sensor, void *arg)
{
  sns_find_sensor_arg *find_arg = (sns_find_sensor_arg *)arg;

  sns_sensor_uid suid = sns_router_get_remote_sensor_suid(remote_sensor);
	

	
  if(sns_sensor_uid_compare(find_arg->suid, &suid))
  {
		
    find_arg->available = sns_router_get_remote_sensor_available(remote_sensor);
    find_arg->remote_sensor = remote_sensor;
    sns_router_get_remote_sensor_data_type(remote_sensor, find_arg->data_type,
                                sizeof(find_arg->data_type));
    return false;
  }

  return true;
}

/**
 * Log a message describing a new data stream which was just successfully
 * created.
 *
 * @param[i] stream Newly created data stream
 * @param[i] src_sensor Source of data sent on this new stream
 */
static void __attribute__((noinline))
log_stream_create(sns_fw_data_stream const *stream,
                  sns_find_sensor_arg const *src_sensor)
{
  char const *dst_name = NULL;
  if(NULL == src_sensor->remote_sensor)
  {
    if(NULL != stream->dst_instance)
    {
      if(NULL != stream->dst_instance->sensor->diag_config.config)
        dst_name = stream->dst_instance->sensor->diag_config.config->datatype;
  
      SNS_SPRINTF(MED, sns_fw_printf,
                  "Created data stream to Sensor '%s' (" SNS_DIAG_PTR
                  ") from Instance '%s' (" SNS_DIAG_PTR "): " SNS_DIAG_PTR,
                  src_sensor->data_type, src_sensor->sensor, dst_name,
                  stream->dst_instance, stream);
    }
    else if(NULL != stream->dst_sensor)
    {
      if(NULL != stream->dst_sensor->diag_config.config)
        dst_name = stream->dst_sensor->diag_config.config->datatype;
  
      SNS_SPRINTF(MED, sns_fw_printf,
                  "Created data stream to Sensor '%s' (" SNS_DIAG_PTR
                  ") from Sensor '%s' (" SNS_DIAG_PTR "): " SNS_DIAG_PTR,
                  src_sensor->data_type, src_sensor->sensor, dst_name,
                  stream->dst_sensor, stream);
    }
  }
}

static sns_rc __attribute__((noinline))
create_sensor_stream_internal(sns_stream_service *this, sns_sensor *dst_sensor,
                              sns_sensor_uid sensor_uid,
                              struct sns_data_stream **data_stream)
{
  UNUSED_VAR(this);

  if(NULL == dst_sensor)
  {
    SNS_SPRINTF(ERROR, sns_fw_printf, "NULL sensor");
    return SNS_RC_INVALID_VALUE;
  }

  sns_rc rv = SNS_RC_NOT_AVAILABLE;
  sns_find_sensor_arg sensor =
      (sns_find_sensor_arg){.suid = &sensor_uid, .sensor = NULL};
  sns_attr_svc_sensor_foreach(&sns_find_sensor, &sensor);

  if(NULL == sensor.sensor)
  {
    sns_remote_sensor_foreach(&sns_find_remote_sensor, &sensor);
  }
  *data_stream = NULL;
  if((NULL != sensor.sensor) || (NULL != sensor.remote_sensor))
  {
    rv = data_stream_init((sns_fw_sensor *)dst_sensor, NULL, &sensor,
                          (sns_fw_data_stream **)data_stream);
  }

  if(NULL != *data_stream)
  {
    if(SNS_ISLAND_STATE_IN_ISLAND != sns_island_get_island_state())
      log_stream_create((sns_fw_data_stream *)*data_stream, &sensor);
  }
  else
  {
    SNS_PRINTF(MED, sns_fw_printf,
               "Unable to create stream to Sensor " SNS_DIAG_PTR
               " from Sensor " SNS_DIAG_PTR,
               sensor.sensor, dst_sensor);
  }

  if(SNS_ISLAND_STATE_IN_ISLAND ==
         ((sns_fw_sensor *)dst_sensor)->island_operation &&
     !sns_island_is_island_ptr((intptr_t)*data_stream))
  {
    sns_sensor_set_island((sns_fw_sensor *)dst_sensor, false);
    SNS_UPRINTF(MED, sns_fw_printf,
                "Sensor " SNS_DIAG_PTR " marked as ISLAND_DISABLED",
                dst_sensor);
  }

  return rv;
}

/* See sns_stream_service_api::create_sensor_stream */
SNS_SECTION(".text.sns")
static sns_rc create_sensor_stream(sns_stream_service *this,
                                   sns_sensor *dst_sensor,
                                   sns_sensor_uid sensor_uid,
                                   struct sns_data_stream **data_stream)
{
  SNS_ISLAND_EXIT();
  return create_sensor_stream_internal(this, dst_sensor, sensor_uid,
                                       data_stream);
}

static sns_rc __attribute__((noinline))
create_sensor_instance_stream_internal(sns_stream_service *this,
                                       sns_sensor_instance *dst_instance,
                                       sns_sensor_uid sensor_uid,
                                       struct sns_data_stream **data_stream)
{
  UNUSED_VAR(this);
  if(NULL == dst_instance)
  {
    SNS_SPRINTF(ERROR, sns_fw_printf, "NULL instance");
    return SNS_RC_INVALID_VALUE;
  }

  sns_rc rv = SNS_RC_NOT_AVAILABLE;
  sns_find_sensor_arg sensor =
      (sns_find_sensor_arg){.suid = &sensor_uid, .sensor = NULL};
  sns_attr_svc_sensor_foreach(&sns_find_sensor, &sensor);

  if(NULL == sensor.sensor && NULL == sensor.library)
  {
    sns_remote_sensor_foreach(&sns_find_remote_sensor, &sensor);
  }
  *data_stream = NULL;
  if((NULL != sensor.sensor) || (NULL != sensor.remote_sensor))
  {
    rv = data_stream_init(NULL, (sns_fw_sensor_instance *)dst_instance, &sensor,
                          (sns_fw_data_stream **)data_stream);
  }

  if(NULL != *data_stream)
  {
    if(SNS_ISLAND_STATE_IN_ISLAND != sns_island_get_island_state())
      log_stream_create((sns_fw_data_stream *)*data_stream, &sensor);
  }
  else
  {
    SNS_PRINTF(MED, sns_fw_printf,
               "Unable to create stream to Sensor " SNS_DIAG_PTR
               " from Instance " SNS_DIAG_PTR,
               sensor.sensor, dst_instance);
  }

  if(SNS_ISLAND_STATE_IN_ISLAND ==
         ((sns_fw_sensor_instance *)dst_instance)->island_operation &&
     !sns_island_is_island_ptr((intptr_t)*data_stream))
  {
    sns_sensor_instance_set_island((sns_fw_sensor_instance *)dst_instance,
                                   false);
    SNS_UPRINTF(MED, sns_fw_printf, "Instance " SNS_DIAG_PTR " ISLAND_DISABLED",
                dst_instance);
  }
  return rv;
}

/* See sns_stream_service_api::create_sensor_instance_stream */
SNS_SECTION(".text.sns")
static sns_rc create_sensor_instance_stream(
    sns_stream_service *this, sns_sensor_instance *dst_instance,
    sns_sensor_uid sensor_uid, struct sns_data_stream **data_stream)
{
  SNS_ISLAND_EXIT();
  return create_sensor_instance_stream_internal(this, dst_instance, sensor_uid,
                                                data_stream);
}

/* See sns_stream_service_api::remove_stream */
SNS_SECTION(".text.sns")
static sns_rc remove_stream(sns_stream_service *this,
                            struct sns_data_stream *data_stream)
{
  UNUSED_VAR(this);
  sns_rc ret_val = SNS_RC_INVALID_VALUE;
  sns_fw_data_stream *stream = (sns_fw_data_stream *)data_stream;

  if(!sns_island_is_island_ptr((intptr_t)data_stream))
    SNS_ISLAND_EXIT();

  if(sns_data_stream_validate(stream))
  {
    sns_data_stream_deinit(stream);
    ret_val = SNS_RC_SUCCESS;
  }
  else
  {
    SNS_PRINTF(ERROR, sns_fw_printf,
               "Refusing to remove invalid stream 0x" SNS_DIAG_PTR, stream);
  }
  return ret_val;
}

/**
 * Handle a request from the client to destroy a data stream.
 *
 * Remove this client from the source Sensor, and remove all unprocessed
 * events, as the client will no longer be processing them.
 *
 * This request is handled asynchronously to avoid deadlock; a meta-event
 * will be sent back to the client (and handled by the Framework), to finalize
 * freeing the data stream.
 *
 * @note sensor::library::library_lock must be held
 *
 * @param[i] sensor Source sensor of this stream
 * @param[i] request Request message associated with a single data stream
 *
 * @return SNS_RC_SUCCESS
 */
SNS_SECTION(".text.sns")
static sns_rc handle_stream_destroy(sns_sensor *sensor, sns_fw_request *request)
{
  sns_fw_sensor *fw_sensor = (sns_fw_sensor *)sensor;
  sns_fw_data_stream *stream = request->stream;
  sns_fw_request *existing_req = stream->client_request;
  sns_sensor_instance *bad_instance = NULL;
  sns_rc ret_val = SNS_RC_SUCCESS;
  SNS_UPRINTF(MED, sns_fw_printf,
              "handle_stream_destroy: " SNS_DIAG_PTR " req=" SNS_DIAG_PTR
              " inst=" SNS_DIAG_PTR,
              stream, existing_req,
              existing_req ? existing_req->instance : NULL);
  sns_osa_lock_acquire(&stream->stream_lock);
  stream->removing = SNS_DATA_STREAM_WAIT_RECEIPT;
  sns_osa_lock_release(&stream->stream_lock);

  if(NULL != existing_req)
  {
    if(!sns_island_is_island_ptr(
           (intptr_t)sensor->sensor_api->set_client_request) ||
       (NULL != sensor->instance_api &&
        !sns_island_is_island_ptr(
            (intptr_t)sensor->instance_api->set_client_config)))
    {
      SNS_ISLAND_EXIT();
    }

    if(NULL != stream->client_request->instance)
    {
      sensor->sensor_api->set_client_request(
          sensor, (sns_request *)stream->client_request, NULL, true);

      if(0 == fw_sensor->active_req_cnt)
      {
        // If there are no requests active on the sensor, disable sensor island
        sns_vote_sensor_island_mode((sns_sensor *)fw_sensor,
                                    SNS_ISLAND_DISABLE);
      }
    }

    sns_fw_sensor *fw_sensor = (sns_fw_sensor *)sensor;
    bad_instance = sns_sensor_util_find_instance(
        sensor, (sns_request *)existing_req, fw_sensor->diag_config.suid);
    if(bad_instance != &sns_instance_no_error)
    {
      SNS_VERBOSE_ASSERT(NULL == bad_instance,
                         "Instance did not remove request!");
    }

    sns_free(existing_req);
    stream->client_request = NULL;
  }

  sns_es_send_event(stream, SNS_FW_MSGID_SNS_DESTROY_COMPLETE_EVENT, 0, NULL);

  return ret_val;
}

/**
 * Handle a non-specialized request, incoming to some Sensor.
 *
 * @note library::library_lock must be held
 *
 * @param[i] sensor Destination of the request
 * @param[i] pend_req Request sent to sensor
 *
 * @return
 * SNS_RC_SUCCESS
 */
SNS_SECTION(".text.sns")
static sns_rc handle_req(sns_sensor *sensor, sns_fw_request *pend_req)
{
  sns_fw_sensor *fw_sensor = (sns_fw_sensor *)sensor;

  struct sns_fw_request *exist_request = pend_req->stream->client_request;
  sns_sensor_instance *instance = &sns_instance_no_error;
  sns_fw_data_stream *data_stream = (sns_fw_data_stream *)pend_req->stream;
  sns_rc rc = SNS_RC_SUCCESS;

  sns_isafe_sensor_access(fw_sensor);

  if(NULL == sensor->sensor_api->set_client_request)
  {
    SNS_SPRINTF(ERROR, sns_fw_printf, "NULL set_client_request");
    return SNS_RC_INVALID_VALUE;
  }

  if(SNS_DATA_STREAM_AVAILABLE != data_stream->removing)
  {
    SNS_PRINTF(HIGH, sensor,
               "Unavailable stream " SNS_DIAG_PTR "; Sensor " SNS_DIAG_PTR,
               data_stream, sensor);
  }
  else
  {
    struct sns_fw_request *added_request = NULL;
    if(!sns_island_is_island_ptr(
           (intptr_t)sensor->sensor_api->set_client_request) ||
       (NULL != sensor->instance_api &&
        !sns_island_is_island_ptr(
            (intptr_t)sensor->instance_api->set_client_config)) ||
       !sns_island_is_island_ptr((intptr_t)exist_request))
    {
      SNS_ISLAND_EXIT();
    }

    rc = sns_fw_log_sensor_api_req(sensor, pend_req);

    if((NULL != exist_request) &&
       (sns_isafe_list_item_present(&exist_request->stream->list_entry_source)))
    {
      added_request = exist_request;
    }

    if(0 == fw_sensor->active_req_cnt)
    {
      // If this is the first request for the sensor, enable sensor island
      sns_vote_sensor_island_mode(sensor, SNS_ISLAND_ENABLE);
    }

    instance = sensor->sensor_api->set_client_request(
        sensor, (sns_request *)added_request, (sns_request *)pend_req, false);

    if(0 == fw_sensor->active_req_cnt)
    {
      // If there are no requests active on the sensor, disable sensor island
      sns_vote_sensor_island_mode(sensor, SNS_ISLAND_DISABLE);
    }
  }

  if(&sns_instance_no_error == instance || NULL == instance)
  {
    if(NULL == instance)
      sns_es_send_error_event(pend_req->stream, SNS_RC_NOT_SUPPORTED);
  }
  else
  {
    bool valid_req = pend_req->stream->client_request == pend_req ||
                     pend_req->stream->client_request == exist_request;
    bool req_found = false;

    sns_fw_sensor *fw_sensor = (sns_fw_sensor *)sensor;
    sns_sensor_uid const *suid = fw_sensor->diag_config.suid;

    for(sns_request const *request =
            instance->cb->get_client_request(instance, suid, true);
        NULL != request;
        request = instance->cb->get_client_request(instance, suid, false))
    {
      if((sns_fw_request *)request == pend_req ||
         (sns_fw_request *)request == exist_request)
      {
        req_found = true;
        break;
      }
    }
    sns_fw_log_inst_map((sns_sensor_instance *)instance);

    SNS_VERBOSE_ASSERT(valid_req, "Stream with invalid client request");
    SNS_VERBOSE_ASSERT(req_found,
                       "Request " SNS_DIAG_PTR
                       " not found on Instance " SNS_DIAG_PTR,
                       pend_req, instance);
  }

  // If the existing request was replaced
  if(pend_req->stream->client_request != exist_request)
    sns_free(exist_request);
  // If the new request was not saved (e.g. flush request)
  if(pend_req->stream->client_request != pend_req)
    sns_free(pend_req);

  return rc;
}

static const sns_msg_handler msg_handlers[] SNS_SECTION(".rodata.sns") = {
    {SNS_STD_MSGID_SNS_STD_ATTR_REQ, &sns_attr_svc_handle_req},
    {SNS_FW_MSGID_SNS_DESTROY_REQ, &handle_stream_destroy}};

/**
 * Process a single pending request.
 *
 * See sns_thread_mgr_task_func().
 */
SNS_SECTION(".text.sns")
static sns_rc sns_stream_service_process(void *func_arg)
{
  sns_fw_request *pend_req = func_arg;
  uint32_t i;
  sns_sensor *sensor;
  bool sensor_found = false;
  sns_rc rc = SNS_RC_SUCCESS;

  if(!sns_island_is_island_ptr((intptr_t)pend_req) ||
     !sns_island_is_island_ptr((intptr_t)pend_req->stream) ||
     SNS_ISLAND_STATE_IN_ISLAND != pend_req->stream->island_operation)
    SNS_ISLAND_EXIT();

  if(!sns_data_stream_validate(pend_req->stream))
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "process:: invalid stream %X",
               pend_req->stream);
    return SNS_RC_INVALID_STATE;
  }

  if(!pend_req->stream->is_dynamic_lib ||
     sns_sensor_dynamic_library_validate(pend_req->stream->source_library))
  {
    sns_isafe_list_iter iter;
    for(sns_isafe_list_iter_init(
            &iter, &pend_req->stream->source_library->sensors, true);
        NULL != sns_isafe_list_iter_curr(&iter);
        sns_isafe_list_iter_advance(&iter))
    {
      sns_fw_sensor *sensor =
          (sns_fw_sensor *)sns_isafe_list_iter_get_curr_data(&iter);
      if(pend_req->stream->data_source == sensor)
      {
        sensor_found = true;
        break;
      }
    }
  }
  else
  {
    SNS_PRINTF(ERROR, sns_fw_printf,
               "process:: invalid lib " SNS_DIAG_PTR
               " for stream " SNS_DIAG_PTR,
               pend_req->stream->source_library, pend_req->stream);
  }

  if(sensor_found)
  {
    sensor = &pend_req->stream->data_source->sensor;
    SNS_PRINTF(LOW, sns_fw_printf, "Process request %X for Sensor %X on %X",
               pend_req, sensor, pend_req->stream);

    for(i = 0; i < ARR_SIZE(msg_handlers); i++)
    {
      if(msg_handlers[i].msg_id == pend_req->request.message_id)
      {
        SNS_ISLAND_EXIT();
        rc = sns_fw_log_sensor_api_req(sensor, pend_req);
        msg_handlers[i].handle_req(sensor, pend_req);

        sns_free(pend_req);
        break;
      }
    }
    if(ARR_SIZE(msg_handlers) == i)
      handle_req(sensor, pend_req);
  }
  else if(SNS_FW_MSGID_SNS_DESTROY_REQ == pend_req->request.message_id)
  {
    sns_osa_lock_acquire(&pend_req->stream->stream_lock);
    pend_req->stream->removing = SNS_DATA_STREAM_WAIT_RECEIPT;
    sns_osa_lock_release(&pend_req->stream->stream_lock);
    sns_es_send_event(pend_req->stream, SNS_FW_MSGID_SNS_DESTROY_COMPLETE_EVENT,
                      0, NULL);
  }
  else
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "process:: msg=%u lib=%X sensor %X != %X",
               pend_req->request.message_id, pend_req->stream->source_library,
               pend_req->stream->source_library->sensors,
               pend_req->stream->data_source);
    sns_es_send_error_event(pend_req->stream, SNS_RC_INVALID_STATE);
    sns_free(pend_req);
  }

  return rc;
}

SNS_SECTION(".text.sns")
static bool validate_source_library(struct sns_fw_data_stream *stream,
                                    struct sns_request *request)
{
  bool lib_valid = true;
  if(stream->source_library->is_dynamic_lib &&
     !sns_sensor_dynamic_library_validate(stream->source_library))
  {
    SNS_PRINTF(
        ERROR, sns_fw_printf,
        "validate_source_library:: request %u for invalid library " SNS_DIAG_PTR
        " on stream " SNS_DIAG_PTR " (removing=%u)",
        request->message_id, stream->source_library, stream, stream->removing);
    if(SNS_FW_MSGID_SNS_DESTROY_REQ == request->message_id)
    {
      sns_osa_lock_acquire(&stream->stream_lock);
      stream->removing = SNS_DATA_STREAM_WAIT_RECEIPT;
      sns_osa_lock_release(&stream->stream_lock);
      sns_es_send_event(stream, SNS_FW_MSGID_SNS_DESTROY_COMPLETE_EVENT, 0,
                        NULL);
    }
    else
    {
      sns_es_send_error_event(stream, SNS_RC_INVALID_STATE);
    }
    lib_valid = false;
  }
  return lib_valid;
}

/*=============================================================================
  Public Function Definitions
  ===========================================================================*/
sns_fw_stream_service *sns_stream_service_init(void)
{
  stream_service.service.service.version = version;
  stream_service.service.service.type = SNS_STREAM_SERVICE;
  stream_service.service.api = &stream_service_api;

  return &stream_service;
}

SNS_SECTION(".text.sns")
bool sns_data_stream_validate_request(sns_fw_request *request)
{
  if(NULL != request)
  {
    return (SNS_STREAM_SERVICE_REQUEST_SANITY ==
            (request->sanity ^ (uint32_t)request->request.request_len ^
             (uint32_t)(uintptr_t)request));
  }
  return false;
}

SNS_SECTION(".text.sns")
void sns_stream_service_add_request(struct sns_fw_data_stream *stream,
                                    struct sns_request *request)
{
  sns_fw_request *pending_req = NULL;
  sns_rc rc = SNS_RC_FAILED;
  sns_tmgr_task_args task_args;
  size_t pending_req_size =
      ALIGN_8(sizeof(*pending_req)) + request->request_len;
  bool island_req;

  if(!sns_island_is_island_ptr((intptr_t)stream) ||
     SNS_ISLAND_STATE_IN_ISLAND != stream->island_operation)
    SNS_ISLAND_EXIT();

  if((NULL == stream->remote_stream) &&
     !validate_source_library(stream, request))
  {
    return;
  }

  // If the source supports handling requests in island mode, attempt to provide
  // this request in island mode so that the source can iterate over all client
  // requests in island mode.
  island_req = stream->island_req &&
               request->request_len < SNS_STREAM_MAX_ISLAND_REQ_LEN;

  if(island_req)
    pending_req = sns_malloc(SNS_HEAP_ISLAND, pending_req_size);
  if(NULL == pending_req)
  {
    SNS_ISLAND_EXIT();
    pending_req = sns_malloc(SNS_HEAP_MAIN, pending_req_size);
  }

  SNS_PRINTF(MED, sns_fw_printf,
             "Add request " SNS_DIAG_PTR " on stream " SNS_DIAG_PTR
             " (length %i; ID %i)",
             pending_req, stream, (int)request->request_len,
             request->message_id);

  SNS_ASSERT(NULL != pending_req);
  pending_req->request.request =
      (void *)(((uint8_t *)pending_req) + ALIGN_8(sizeof(*pending_req)));

  pending_req->request.request_len = request->request_len;
  pending_req->request.message_id = request->message_id;
  pending_req->stream = (sns_fw_data_stream *)stream;
  pending_req->instance = NULL;
  sns_memscpy(pending_req->request.request, pending_req->request.request_len,
              request->request, request->request_len);

  request_add_checksum(pending_req);

  if(NULL != stream->remote_stream)
  {
    rc = sns_router_send_request(stream, pending_req);
    SNS_PRINTF(MED, sns_fw_printf,
             "Add request " SNS_DIAG_PTR " on stream " SNS_DIAG_PTR
             " on remote_hub_conn " SNS_DIAG_PTR "rc=%u",
             pending_req, stream, stream->remote_stream, rc);
  }
  else
  {
    task_args.task_lock = stream->source_library->library_lock;
    task_args.instance = NULL;

    sns_thread_manager_add(stream->source_library, &sns_stream_service_process,
      pending_req, stream->req_priority, &task_args);
  }
}

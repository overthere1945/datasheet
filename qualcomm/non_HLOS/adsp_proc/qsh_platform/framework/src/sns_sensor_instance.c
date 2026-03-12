/*=============================================================================
  @file sns_sensor_instance.c

  All functionality related to the creation, deletion, and modification of
  Sensor Instances.

  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  ===========================================================================*/

/*=============================================================================
  Include Files
  ===========================================================================*/

#include "sns_assert.h"
#include "sns_diag_print.h"
#include "sns_fw_basic_services.h"
#include "sns_fw_data_stream.h"
#include "sns_fw_event_service.h"
#include "sns_fw_log.h"
#include "sns_fw_memory_service.h"
#include "sns_fw_request.h"
#include "sns_fw_sensor.h"
#include "sns_fw_sensor_instance.h"
#include "sns_fw_service_manager.h"
#include "sns_isafe_list.h"
#include "sns_island.h"
#include "sns_memmgr.h"
#include "sns_osa_lock.h"
#include "sns_printf_int.h"
#include "sns_pwr_sleep_mgr.h"
#include "sns_rc.h"
#include "sns_sensor.h"
#include "sns_sensor_instance.h"
#include "sns_types.h"

#include "sns_fw_diag_service.h"

/*=============================================================================
  Type Definitions
  ===========================================================================*/

typedef struct sns_sensor_instance_state_init
{
  sns_rc (*init)(sns_fw_sensor_instance *instance, sns_fw_sensor *sensor);
  void (*deinit)(sns_fw_sensor_instance *instance);
} sns_sensor_instance_state_init;

/*=============================================================================
  Static Data Definitions
  ===========================================================================*/

static sns_sensor_instance_cb instance_cb SNS_SECTION(".data.sns");

/**
 * Each entry is called once per sensor instance init/deinit.
 * Called functions are intended to: deinitialize any state stored within the
 * sensor instance, and/or remove any separately managed state associated
 * with this Sensor Instance.
 */
static sns_sensor_instance_state_init state_init[] SNS_SECTION(".data.sns") = {
    {.init = &sns_diag_sensor_instance_init,
     .deinit = &sns_diag_sensor_instance_deinit},
    {.init = NULL, .deinit = &sns_es_deinit_instance},
};

/*=============================================================================
  Static Function Definitions
  ===========================================================================*/

/* See sns_sensor_instance_cb::get_service_manager */
SNS_SECTION(".text.sns")
static sns_service_manager *get_service_manager(sns_sensor_instance *this)
{
  UNUSED_VAR(this);
  return (sns_service_manager *)sns_service_manager_ref();
}

/**
 * Find the client request list of a Sensor Instance associated with a
 * particular SUID.
 *
 * @return Found request list; or NULL if not found
 */
SNS_SECTION(".text.sns")
static sns_client_req_list *find_req_list(sns_fw_sensor_instance *this,
                                          sns_sensor_uid const *suid)
{
  sns_isafe_list_iter iter;

  for(sns_isafe_list_iter_init(&iter, &this->client_req_lists, true);
      NULL != sns_isafe_list_iter_curr(&iter);
      sns_isafe_list_iter_advance(&iter))
  {
    sns_client_req_list *req_list =
        (sns_client_req_list *)sns_isafe_list_iter_get_curr_data(&iter);

    if(suid == NULL || sns_sensor_uid_compare(suid, req_list->suid))
      return req_list;
  }

  return NULL;
}

/**
 * See sns_sensor_instance_cb::get_client_request
 */
SNS_SECTION(".text.sns")
static sns_request const *get_client_request(sns_sensor_instance *this,
                                             sns_sensor_uid const *suid,
                                             bool first)
{
  sns_request *rv = NULL;
  sns_fw_sensor_instance *instance = (sns_fw_sensor_instance *)this;
  sns_client_req_list *req_list = NULL;

  req_list = find_req_list(instance, suid);

  if(NULL != req_list)
  {
    sns_isafe_list_iter *iter = &req_list->iter;
    sns_isafe_list_item *item = NULL;

    if(first)
    {
      sns_isafe_list_iter_init(iter, &req_list->client_requests, true);
      sns_isafe_list_iter_set_always_check_island_ptrs(iter, true);
      item = sns_isafe_list_iter_curr(iter);
    }
    else if(NULL != iter->list)
    {
      item = sns_isafe_list_iter_advance(iter);
    }

    if(NULL != item)
    {
      sns_fw_data_stream *stream =
          (sns_fw_data_stream *)sns_isafe_list_item_get_data(item);
      if(NULL != stream && NULL != stream->client_request)
      {
        rv = &stream->client_request->request;
        // SNS_ASSERT(sns_data_stream_validate_request((sns_fw_request*)rv));
      }
    }
  }

  if(!sns_island_is_island_ptr((intptr_t)rv))
    SNS_ISLAND_EXIT();

  return rv;
}

/**
 *  See sns_sensor_instance_cb::remove_client_request
 */
SNS_SECTION(".text.sns")
static void remove_client_request(sns_sensor_instance *this,
                                  struct sns_request const *request)
{
  sns_fw_sensor_instance *instance = (sns_fw_sensor_instance *)this;
  sns_fw_sensor *fw_sensor = (sns_fw_sensor *)instance->sensor;

  if(sns_data_stream_validate_request((sns_fw_request *)request))
  {
    sns_isafe_list_iter req_list_iter, req_iter;
    bool done = false;

    for(sns_isafe_list_iter_init(&req_list_iter, &instance->client_req_lists,
                                 true);
        NULL != sns_isafe_list_iter_curr(&req_list_iter) && !done;
        sns_isafe_list_iter_advance(&req_list_iter))
    {
      sns_client_req_list *req_list =
          (sns_client_req_list *)sns_isafe_list_iter_get_curr_data(
              &req_list_iter);

      for(sns_isafe_list_iter_init(&req_iter, &req_list->client_requests, true);
          NULL != sns_isafe_list_iter_curr(&req_iter) && !done;
          sns_isafe_list_iter_advance(&req_iter))
      {
        sns_fw_data_stream *stream =
            (sns_fw_data_stream *)sns_isafe_list_iter_get_curr_data(&req_iter);

        if(&stream->client_request->request == request)
        {
          if(!sns_island_is_island_ptr((intptr_t)req_list->active_cluster))
            SNS_ISLAND_EXIT();

          sns_osa_lock_acquire(&event_service.lock);
          sns_es_release_cluster(req_list->active_cluster);
          sns_osa_lock_release(&event_service.lock);

          req_list->active_cluster = NULL;

          // Setting the instance to a valid, non-NULL value so that
          // stream destroy can be handled properly
          sns_isafe_list_iter_remove(&req_iter);
          ((sns_fw_request *)request)->instance = &sns_instance_no_error;
          done = true;
        }
      }
      if(0 == req_list->client_requests.cnt)
      {
        sns_pwr_sleep_mgr_remove_config(req_list->suid, this);
        sns_isafe_list_iter_remove(&req_list_iter);

        // If instance not in island, and freed request list not in island,
        // try setting island
        if(sns_island_is_island_ptr((intptr_t)instance) &&
           SNS_ISLAND_STATE_IN_ISLAND != instance->island_operation &&
           !sns_island_is_island_ptr((intptr_t)req_list))
        {
          sns_sensor_instance_set_island(instance, true);
        }
        sns_free(req_list);
      }
    }
  }

  if(0 == sns_get_sensor_active_req_cnt(fw_sensor))
  {
    // If there are no requests active on the sensor, disable sensor island
    sns_vote_sensor_island_mode((sns_sensor *)fw_sensor, SNS_ISLAND_DISABLE);
  }
  sns_fw_log_inst_map(this);
}

/**
 *  See sns_sensor_instance_cb::add_client_request
 */
SNS_SECTION(".text.sns")
static void add_client_request(sns_sensor_instance *this,
                               struct sns_request const *request)
{
  sns_isafe_list_iter iter;
  sns_fw_sensor_instance *instance = (sns_fw_sensor_instance *)this;
  sns_fw_sensor *fw_sensor = (sns_fw_sensor *)instance->sensor;
  sns_fw_request *fw_request = (sns_fw_request *)request;

  // SNS_VERBOSE_ASSERT(sns_data_stream_validate_request((sns_fw_request*)fw_request),
  //                    "add_client_request() called with invalid request");

  // If the stream has been removed on which the request came in then assert.
  SNS_ASSERT(SNS_DATA_STREAM_WAIT_RECEIPT != fw_request->stream->removing);

  if(!sns_island_is_island_ptr((intptr_t)fw_request->stream) ||
     SNS_ISLAND_STATE_IN_ISLAND != fw_request->stream->island_operation)
  {
    SNS_ISLAND_EXIT();
  }

  SNS_VERBOSE_ASSERT((SNS_INST_REMOVING_NOT_START == instance->inst_removing),
                     "Adding request to removed instance");

  sns_sensor *src_sensor = &fw_request->stream->data_source->sensor;
  sns_sensor_uid const *suid =
      ((struct sns_fw_sensor *)src_sensor)->diag_config.suid;
  sns_client_req_list *req_list = find_req_list(instance, suid);
  bool island_operation =
      (SNS_ISLAND_STATE_NOT_IN_ISLAND != instance->island_operation);

  if(NULL == req_list)
  {
    if(island_operation)
    {
      req_list = sns_malloc(SNS_HEAP_ISLAND, sizeof(*req_list));
    }
    if(NULL == req_list)
    {
      SNS_ISLAND_EXIT();
      req_list = sns_malloc(SNS_HEAP_MAIN, sizeof(*req_list));
      if(SNS_ISLAND_STATE_NOT_IN_ISLAND != instance->island_operation)
      {
        instance->island_operation = SNS_ISLAND_STATE_ISLAND_DISABLED;
      }
    }
    SNS_ASSERT(NULL != req_list);

    req_list->suid = suid;
    sns_isafe_list_init(&req_list->client_requests);

    sns_isafe_list_item_init(&req_list->list_entry, req_list);
    sns_isafe_list_iter_init(&iter, &instance->client_req_lists, true);
    sns_isafe_list_iter_insert(&iter, &req_list->list_entry, true);
  }

  if(!sns_island_is_island_ptr((intptr_t)req_list->active_cluster))
    SNS_ISLAND_EXIT();

  sns_osa_lock_acquire(&event_service.lock);
  sns_es_release_cluster(req_list->active_cluster);
  sns_osa_lock_release(&event_service.lock);

  req_list->active_cluster = NULL;

  // The stream might have been previously added to a different instance.
  // Remove it from any instance to be safe.
  if(sns_isafe_list_item_present(&fw_request->stream->list_entry_source))
    sns_isafe_list_item_remove(&fw_request->stream->list_entry_source);

  sns_isafe_list_iter_init(&iter, &req_list->client_requests, false);
  sns_isafe_list_iter_insert(&iter, &fw_request->stream->list_entry_source,
                             true);

  // The previous request will not be freed here: that will be done within
  // handle_req of the Stream Service.
  fw_request->stream->client_request = fw_request;

  // Cross reference instance with request
  fw_request->instance = this;

  if(0 < sns_get_sensor_active_req_cnt(fw_sensor))
  {
    // If there is at least one request active on the sensor, enable sensor
    // island
    sns_vote_sensor_island_mode((sns_sensor *)fw_sensor, SNS_ISLAND_ENABLE);
  }
}

/*
 *  Find total number of requests present on the sensor instance
 */
static uint32_t find_num_req(sns_fw_sensor_instance *this)
{
  uint32_t count = 0;
  sns_isafe_list_iter iter;
  for(sns_isafe_list_iter_init(&iter, &this->client_req_lists, true);
      NULL != sns_isafe_list_iter_curr(&iter);
      sns_isafe_list_iter_advance(&iter))
  {
    sns_client_req_list *req_list =
        (sns_client_req_list *)sns_isafe_list_iter_get_curr_data(&iter);
    count += req_list->client_requests.cnt;
  }
  return count;
}

/**
 * Alloc a sensor instance
 */
SNS_SECTION(".text.sns")
static sns_fw_sensor_instance *
sns_sensor_instance_alloc(sns_fw_sensor *fw_sensor, size_t state_size,
                          size_t diag_size)
{
  sns_fw_sensor_instance *instance = NULL;
  sns_sensor_instance_state *inst_state_ptr = NULL;
  sns_mem_heap_id heap_id;
  sns_mem_segment mem_segment = fw_sensor->library->mem_segment;
  size_t fw_instance_size =
      sns_cstruct_extn_compute_total_size(sizeof(*instance), 1, diag_size);
  struct sns_sensor_instance_api const *instance_api =
      fw_sensor->sensor.instance_api;
  bool island_instance =
      (NULL == instance_api ||
       (sns_island_is_island_ptr((intptr_t)instance_api) &&
        sns_island_is_island_ptr((intptr_t)instance_api->notify_event)));

  if(island_instance)
  {
    heap_id = sns_memmgr_get_sensor_heap_id(mem_segment);

    instance = sns_malloc(SNS_HEAP_ISLAND, fw_instance_size);
    if(NULL != instance)
    {
      inst_state_ptr = sns_malloc(heap_id, state_size);
      if(NULL == inst_state_ptr)
      {
        sns_free(instance);
        instance = NULL;
      }
    }
  }

  if(NULL == instance)
  {
    SNS_ISLAND_EXIT();
    instance = sns_malloc(SNS_HEAP_MAIN, fw_instance_size);
    inst_state_ptr = sns_malloc(SNS_HEAP_MAIN, state_size);
  }
  SNS_ASSERT(NULL != instance);
  SNS_ASSERT(NULL != inst_state_ptr);
  instance->instance.state = inst_state_ptr;
  return instance;
}

/*=============================================================================
  Public Function Definitions
  ===========================================================================*/

sns_rc sns_sensor_instance_init_fw(void)
{
  instance_cb =
      (sns_sensor_instance_cb){.struct_len = sizeof(instance_cb),
                               .get_service_manager = &get_service_manager,
                               .get_client_request = &get_client_request,
                               .remove_client_request = &remove_client_request,
                               .add_client_request = &add_client_request};

  return SNS_RC_SUCCESS;
}

SNS_SECTION(".text.sns")
sns_sensor_instance *sns_sensor_instance_init_v2(sns_sensor *sensor,
                                                 uint32_t state_len,
                                                 uint32_t state_len_ni)
{
  sns_fw_sensor *fw_sensor = (sns_fw_sensor *)sensor;
  sns_fw_sensor_instance *instance = NULL;
  sns_isafe_list_iter iter;
  sns_rc rv = SNS_RC_SUCCESS;
  uint32_t sensors_cnt = fw_sensor->library->sensors.cnt;
  size_t state_size = ALIGN_8(state_len) + sizeof(sns_sensor_instance_state);
  size_t state_ni_size =
      ALIGN_8(state_len_ni) + sizeof(sns_sensor_instance_state);
  size_t diag_size = ALIGN_8(sizeof(sns_diag_instance_config)) * sensors_cnt;

  instance = sns_sensor_instance_alloc(fw_sensor, state_size, diag_size);

  instance->instance.cb = &instance_cb;

  sns_cstruct_extn_init(&instance->extn, instance, sizeof(*instance), 1);
  instance->diag_header.cstruct_enxtn_idx =
      sns_cstruct_extn_setup_buffer(&instance->extn, diag_size);
  instance->diag_header.datatypes_cnt = sensors_cnt;

  if(state_len_ni)
  {
    SNS_ISLAND_EXIT();
    instance->instance.state_ni = sns_malloc(SNS_HEAP_MAIN, state_ni_size);
    SNS_ASSERT(NULL != instance->instance.state_ni);
    instance->instance.state_ni->state_len = state_len_ni;
  }
  else
  {
    instance->instance.state_ni = NULL;
  }
  instance->instance.state->state_len = state_len;
  instance->sensor = fw_sensor;
  instance->event = NULL;
  instance->inst_removing = SNS_INST_REMOVING_NOT_START;
  instance->in_use = false;

  sns_isafe_list_init(&instance->client_req_lists);
  sns_isafe_list_init(&instance->data_streams);
  sns_isafe_list_item_init(&instance->list_entry, instance);

  if(sns_island_is_island_ptr((intptr_t)instance))
  {
    // island instance is added to head of list
    instance->island_operation = SNS_ISLAND_STATE_IN_ISLAND;
    sns_isafe_list_iter_init(&iter, &fw_sensor->sensor_instances, true);
    sns_isafe_list_iter_insert(&iter, &instance->list_entry, false);
  }
  else
  {
    // big image instance is added to tail of list
    instance->island_operation = SNS_ISLAND_STATE_NOT_IN_ISLAND;
    sns_isafe_list_iter_init(&iter, &fw_sensor->sensor_instances, false);
    sns_isafe_list_iter_insert(&iter, &instance->list_entry, true);
  }

  for(uint8_t i = 0; i < ARR_SIZE(state_init); i++)
    if(NULL != state_init[i].init)
      state_init[i].init(instance, fw_sensor);

  if(NULL != sensor->instance_api)
  {
    if(!sns_island_is_island_ptr((intptr_t)sensor->instance_api->init))
      SNS_ISLAND_EXIT();

    rv = sensor->instance_api->init((sns_sensor_instance *)instance,
                                    sensor->state);

    if(SNS_RC_SUCCESS != rv)
    {
      SNS_PRINTF(ERROR, sns_fw_printf, "instance_api->init error %i", rv);
      sns_sensor_instance_deinit((sns_sensor_instance *)instance);
      instance = NULL;
    }
    else
    {
      SNS_UPRINTF(LOW, sns_fw_printf,
                  "Created new Sensor Instance " SNS_DIAG_PTR
                  " from Sensor " SNS_DIAG_PTR " with Island operation: %d",
                  instance, sensor, instance->island_operation);
    }
  }

  return (sns_sensor_instance *)instance;
}

SNS_SECTION(".text.sns")
sns_sensor_instance *sns_sensor_instance_init(sns_sensor *sensor,
                                              uint32_t state_len)
{
  return sns_sensor_instance_init_v2(sensor, state_len, 0);
}

SNS_SECTION(".text.sns")
void sns_sensor_instance_delete(sns_fw_sensor_instance *this)
{
  sns_isafe_list_iter *iter;
  SNS_UPRINTF(MED, sns_fw_printf, "sns_sensor_instance_delete " SNS_DIAG_PTR,
              this);

  iter = &this->sensor->instances_iter;
  if(this->sensor->library->is_multithreaded)
  {
    sns_osa_lock_acquire(this->sensor->sensor_instances_list_lock);
  }
  if(NULL != sns_isafe_list_iter_curr(iter) &&
     (void *)this == sns_isafe_list_iter_get_curr_data(iter))
  {
    sns_isafe_list_iter_remove(iter);
    sns_isafe_list_iter_return(iter);
  }
  else
  {
    sns_isafe_list_item_remove(&this->list_entry);
  }
  if(this->sensor->library->is_multithreaded)
  {
    sns_osa_lock_release(this->sensor->sensor_instances_list_lock);
  }
  sns_sensor_delete(this->sensor);

  if(NULL != this->instance.state_ni)
  {
    SNS_ISLAND_EXIT();
    SNS_PRINTF(HIGH, sns_fw_printf, "free state_ni of instance " SNS_DIAG_PTR,
               this);
    sns_free(this->instance.state_ni);
  }
  sns_free(this->instance.state);
  sns_free(this);
}

SNS_SECTION(".text.sns")
void sns_sensor_instance_deinit(sns_sensor_instance *instance)
{
  sns_fw_sensor_instance *this = (sns_fw_sensor_instance *)instance;
  sns_fw_sensor *fw_sensor = (sns_fw_sensor *)this->sensor;
  sns_isafe_list_iter iter;
  sns_sensor *sensor = &this->sensor->sensor;

  if(SNS_INST_REMOVING_NOT_START != this->inst_removing)
  {
    SNS_UPRINTF(HIGH, sns_fw_printf,
                "sns_sensor_instance_deinit: already started " SNS_DIAG_PTR,
                this);
    return;
  }

  SNS_UPRINTF(MED, sns_fw_printf, "sns_sensor_instance_deinit " SNS_DIAG_PTR,
              this);
  this->inst_removing = SNS_INST_REMOVING_START;

  if(NULL != sensor->instance_api && NULL != sensor->instance_api->deinit)
  {
    if(!sns_island_is_island_ptr((intptr_t)sensor->instance_api->deinit))
      SNS_ISLAND_EXIT();
    sensor->instance_api->deinit((sns_sensor_instance *)this);
  }

  sns_memory_service_cleanup(instance);

  // In most cases, the Instance will be removed once it has no clients, but
  // upon an error, we need to send the appropriate event to those that remain
  sns_es_publish_error(this, SNS_RC_INVALID_STATE);

  // Data streams (and their associated client requests) are freed by their
  // client; here we simply remove references to them from this source instance
  sns_isafe_list_iter_init(&iter, &this->client_req_lists, true);
  while(NULL != sns_isafe_list_iter_curr(&iter))
  {
    sns_client_req_list *req_list =
        (sns_client_req_list *)sns_isafe_list_iter_get_curr_data(&iter);
    sns_isafe_list_iter temp;

    sns_isafe_list_iter_init(&temp, &req_list->client_requests, true);
    while(NULL != sns_isafe_list_iter_curr(&temp))
    {
      sns_fw_data_stream *stream =
          (sns_fw_data_stream *)sns_isafe_list_iter_get_curr_data(&temp);
      sns_isafe_list_iter_remove(&temp);

      if(NULL != stream && NULL != stream->client_request)
      {
        stream->client_request->instance = NULL;
      }
    }

    if(!sns_island_is_island_ptr((intptr_t)req_list->active_cluster))
      SNS_ISLAND_EXIT();

    sns_osa_lock_acquire(&event_service.lock);
    sns_es_release_cluster(req_list->active_cluster);
    sns_osa_lock_release(&event_service.lock);

    sns_isafe_list_iter_remove(&iter);
    sns_free(req_list);
  }
  
  if(0 == sns_get_sensor_active_req_cnt(fw_sensor))
  {
    // If there are no requests active on the sensor, disable sensor island
    sns_vote_sensor_island_mode((sns_sensor *)fw_sensor, SNS_ISLAND_DISABLE);
  }

  for(size_t i = 0; i < ARR_SIZE(state_init); i++)
    if(NULL != state_init[i].deinit)
      state_init[i].deinit(this);

  // remove the config from sns_pwr_sleep_mgr's list for all datatypes
  for(int i = 0; i < this->diag_header.datatypes_cnt; i++)
  {
    sns_diag_instance_config *config =
        (sns_diag_instance_config *)sns_cstruct_extn_get_buffer(
            &this->extn, this->diag_header.cstruct_enxtn_idx);
    if(NULL != config)
    {
      sns_pwr_sleep_mgr_remove_config(config[i].suid,
                                      (sns_sensor_instance *)this);
    }
  }

  sns_isafe_list_iter_init(&iter, &this->data_streams, true);
  if(0 == sns_isafe_list_iter_len(&iter))
  {
    sns_sensor_instance_delete(this);
  }
  else
  {
    for(; NULL != sns_isafe_list_iter_curr(&iter);
        sns_isafe_list_iter_advance(&iter))
    {
      struct sns_fw_data_stream *stream =
          (struct sns_fw_data_stream *)sns_isafe_list_iter_get_curr_data(&iter);
      this->inst_removing = SNS_INST_REMOVING_WAITING;
      sns_data_stream_deinit(stream);
    }
  }
}

SNS_SECTION(".text.sns")
bool sns_sensor_instance_set_island(sns_fw_sensor_instance *instance,
                                    bool enabled)
{
  bool rv = false;

  if(!enabled)
  {
    if(SNS_ISLAND_STATE_IN_ISLAND == instance->island_operation)
      instance->island_operation = SNS_ISLAND_STATE_ISLAND_DISABLED;
    rv = true;
  }
  else
  {
    if(SNS_ISLAND_STATE_ISLAND_DISABLED == instance->island_operation)
    {
      rv = sns_instance_streams_in_island((sns_sensor_instance *)instance);

      if(rv)
        instance->island_operation = SNS_ISLAND_STATE_IN_ISLAND;
    }
  }

  return rv;
}

SNS_SECTION(".text.sns")
bool sns_instance_streams_in_island(sns_sensor_instance const *instance)
{
  sns_fw_sensor_instance *fw_instance = (sns_fw_sensor_instance *)instance;
  bool rv = true;

  // Check if all incoming streams are in island.
  sns_isafe_list_iter stream_iter;
  if(NULL !=
     sns_isafe_list_iter_init(&stream_iter, &fw_instance->data_streams, true))
  {
    for(sns_fw_data_stream *data_stream =
            sns_isafe_list_iter_get_curr_data(&stream_iter);
        NULL != data_stream; data_stream = sns_isafe_list_item_get_data(
                                 sns_isafe_list_iter_advance(&stream_iter)))
    {
      if(!sns_island_is_island_ptr((intptr_t)data_stream))
      {
        return false;
      }
    }
  }

  // Check if request list for all SUIDs are in island.
  sns_isafe_list_iter req_list_iter;
  for(sns_isafe_list_iter_init(&req_list_iter, &fw_instance->client_req_lists,
                               true);
      NULL != sns_isafe_list_iter_curr(&req_list_iter) && rv;
      sns_isafe_list_iter_advance(&req_list_iter))
  {
    sns_client_req_list *req_list =
        (sns_client_req_list *)sns_isafe_list_iter_get_curr_data(
            &req_list_iter);

    if(!sns_island_is_island_ptr((intptr_t)req_list))
    {
      rv = false;
      break;
    }
  }

  return rv;
}

int sns_sensor_instance_remove_all_requests(sns_fw_sensor_instance *instance)
{
  sns_client_req_list *req_list;
  int num_requests = 0;
  uint32_t total_num_req = 0;

  total_num_req = find_num_req(instance);

  while(0 < total_num_req && NULL != (req_list = find_req_list(instance, NULL)))
  {
    sns_isafe_list_iter *iter = &req_list->iter;
    sns_isafe_list_item *item = NULL;
    sns_isafe_list_iter_init(iter, &req_list->client_requests, true);
    item = sns_isafe_list_iter_curr(iter);

    if(NULL != item)
    {
      sns_fw_data_stream *stream =
          (sns_fw_data_stream *)sns_isafe_list_item_get_data(item);
      if(sns_data_stream_validate(stream) && NULL != stream->client_request)
      {
        sns_sensor *sensor = &stream->data_source->sensor;
        SNS_FW_DBG_LOG("remove_all_requests: stream=%X req=%X", stream,
                       stream->client_request);
        sns_es_send_error_event(stream, SNS_RC_INVALID_STATE);
        sensor->sensor_api->set_client_request(
            sensor, (sns_request *)stream->client_request, NULL, true);
        num_requests++;
        total_num_req--;
      }
    }
  }
  if(0 < num_requests)
  {
    SNS_FW_DBG_LOG("remove_all_requests: removed %u requests from instance %X",
                   num_requests, instance);
  }
  return num_requests;
}

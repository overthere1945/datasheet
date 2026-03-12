/**
 * sns_sync_com_port_spi.c
 *
 * SPI implementation
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#include <stdatomic.h>

#include "sns_assert.h"
#include "sns_async_com_port_int.h"
#include "sns_com_port_clk_resources.h"
#include "sns_com_port_priv.h"
#include "sns_com_port_spi.h"
#include "sns_com_port_types.h"
#include "sns_com_port_profile.h"
#include "sns_island.h"
#include "sns_island_util.h"
#include "sns_math_ops.h"
#include "sns_mem_util.h"
#include "sns_memmgr.h"
#include "sns_osa_sem.h"
#include "sns_osa_lock.h"
#include "sns_printf_int.h"
#include "sns_rc.h"
#include "sns_sync_com_port_service.h"
#include "sns_time.h"
#include "sns_types.h"
#include "spi_api.h"

/**--------------------------------------------------------------------------*/
#define MAX_INSTANCES_SUPPORTED 4 // Maximum number of bus instances

#define SNS_SPI_GET_QUP_SE_INDEX(inst) (uint32_t)(inst - 1)

#define MAX_SPI_DESCRIPTORS                                                    \
  3 // Maximum number of descriptors supported by
    // core bus APIs
/**--------------------------------------------------------------------------*/
typedef struct sns_spi_clk_resource_s
{
  uint8_t bus_instance;
  uint8_t ref_count;
  uint32_t bus_speed_khz;
} sns_spi_clk_resource_s;

typedef struct sns_spi_info
{
  void *spi_handle;
  sns_time txn_start_time;
  _Atomic unsigned int xfer_in_progress;
} sns_spi_info;

/* Matches timestamp type from spi_api.h (version 2 or later) */
typedef enum
{
  SNS_SPI_TS_32 = 1, /**< Returned value is 32 bit timestamp [38:7] of QTimer
                        value as provided by HW.*/
  SNS_SPI_TS_64, /**< Returned value is 64 bit timestamp [63:0] of QTimer value
                    as provided by HW.*/
} sns_spi_ts_type;

_Static_assert((int)SNS_SPI_TS_32 == (int)SPI_TIMESTAMP_32_BIT_LEGACY,
               "Update sns_spi_ts_type to match spi_timestamp_type");
_Static_assert((int)SNS_SPI_TS_64 == (int)SPI_TIMESTAMP_64_BIT,
               "Update sns_spi_ts_type to match spi_timestamp_type");
_Static_assert(sizeof(sns_spi_ts_type) == sizeof(spi_timestamp_type),
               "Update sns_spi_ts_type to match spi_timestamp_type");

SNS_SECTION(".data.sns")
static sns_spi_clk_resource_s sns_spi_clk_resources[MAX_INSTANCES_SUPPORTED];
SNS_SECTION(".data.sns")
static sns_osa_lock sns_spi_clk_resource_lock;
SNS_SECTION(".data.sns")
static _Atomic unsigned int sns_spi_initialized = 0;
SNS_SECTION(".data.sns")
static sns_com_port_config_ex com_port_ex_default = {
    .bus_mode = SNS_BUS_MODE_0,
    .rw_mode = SNS_SPI_R_W_BIT_MODE_0,
    .size_of_this_struct = sizeof(sns_com_port_config_ex),
};

/**--------------------------------------------------------------------------*/
SNS_SECTION(".text.sns")
static void sns_spi_callback(uint32 transfer_status, void *ctxt)
{
  sns_com_port_priv_handle *priv_handle = (sns_com_port_priv_handle *)ctxt;
  sns_com_port_config *com_config = &priv_handle->com_config;
  sns_com_port_vector_xfer_map *vec_map = &priv_handle->xfer_map;
  sns_bus_info *bus_info = &priv_handle->bus_info;
  sns_spi_info *spi_info;
  int num_vectors;
  int register_len = 0;
  sns_port_vector *vector_ptr;
  sns_rc rw_err = SNS_RC_FAILED;
  int i;
  int memory_length_offset = 0;
  uint32_t transferred = 0;
  uint8_t *buffer_ptr;

  if(!sns_island_is_island_ptr((intptr_t)vec_map) ||
     !sns_is_in_pram_region((uint32_t)vec_map->xfer_buffer_ptr) ||
     !sns_island_is_island_ptr((intptr_t)vec_map->orig_vector_ptr) ||
     !sns_island_is_island_ptr((intptr_t)bus_info))
  {
    SNS_ISLAND_EXIT();
  }
  vector_ptr = vec_map->orig_vector_ptr;
  num_vectors = vec_map->num_vectors;
  spi_info = (sns_spi_info *)bus_info->bus_config;

  switch(com_config->reg_addr_type)
  {
  case SNS_REG_ADDR_16_BIT:
    register_len = 2;
    break;
  case SNS_REG_ADDR_32_BIT:
    register_len = 4;
    break;
  default:
  case SNS_REG_ADDR_8_BIT:
    register_len = 1;
    break;
  }

  if(SPI_SUCCESS(transfer_status))
  {
    rw_err = SNS_RC_SUCCESS;
    for(i = 0; i < num_vectors; i++)
    {
      if(!vec_map->orig_vector_ptr[i].is_write &&
         (NULL != vector_ptr[i].buffer))
      {
        // For read operations, copy data from PRAM/DDR back to client buffer
        if(!sns_island_is_island_ptr((intptr_t)vector_ptr[i].buffer))
        {
          SNS_ISLAND_EXIT();
        }
        if(com_config->qup_type == SNS_TOP_QUP || SNS_DDR_FOR_BUS_TRANSACTION)
        {
          buffer_ptr = SNS_GET_CACHE_ALIGNED_PTR(vec_map->xfer_buffer_ptr +
                                                 memory_length_offset);
        }
        else
        {
          buffer_ptr = vec_map->xfer_buffer_ptr + memory_length_offset;
        }
        sns_memscpy(vector_ptr[i].buffer, vector_ptr[i].bytes,
                    buffer_ptr + register_len, vector_ptr[i].bytes);
      }
      memory_length_offset += register_len + vector_ptr[i].bytes;
      transferred += vector_ptr[i].bytes;
    }
  }

  atomic_store(&spi_info->xfer_in_progress, false);

  if(priv_handle->callback != NULL)
  {
    priv_handle->callback(priv_handle, num_vectors, vector_ptr, transferred,
                          rw_err, priv_handle->args);
  }
}

/**--------------------------------------------------------------------------*/
static sns_rc __attribute__((noinline))
sns_open_spi_internal(sns_sync_com_port_handle *port_handle)
{
  sns_com_port_priv_handle *priv_handle =
      (sns_com_port_priv_handle *)port_handle;
  sns_bus_info *bus_info = &priv_handle->bus_info;
  sns_com_port_config *com_config = &priv_handle->com_config;
  sns_rc sns_result;
  spi_status_t result;
  sns_spi_info *spi_info;

  if(com_config->bus_instance < SNS_MIN_BUS_INSTANCE_VALUE)
    return SNS_RC_INVALID_VALUE;
  bus_info->bus_config = sns_malloc(SNS_HEAP_ISLAND, sizeof(sns_spi_info));
  if(NULL == bus_info->bus_config)
  {
    // We ran out of island heap, so let's throw out a warning message and then
    // allocate the spi config in DDR. This isn't ideal for power, but
    // will help with stability.
    SNS_PRINTF(HIGH, sns_fw_printf, "Alloc SPI config in DDR!");
    bus_info->bus_config = sns_malloc(SNS_HEAP_MAIN, sizeof(sns_spi_info));
    SNS_ASSERT(NULL != bus_info->bus_config);
  }

  spi_info = (sns_spi_info *)bus_info->bus_config;

  sns_result = sns_spi_setup_clk_resources(
      com_config->qup_type, com_config->qup_instance,
      (spi_instance_t)(com_config->bus_instance), com_config->max_bus_speed_KHz,
      NULL);
  if(sns_result != SNS_RC_SUCCESS)
  {
    sns_free(bus_info->bus_config);
    bus_info->bus_config = NULL;
    return SNS_RC_FAILED;
  }
  result = spi_open_ex(
      SNS_GET_QUP_TYPE(com_config->qup_type, com_config->qup_instance),
      SNS_SPI_GET_QUP_SE_INDEX(com_config->bus_instance),
      &spi_info->spi_handle);

  if(SPI_ERROR(result))
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "spi_open failure. spi_instance %d err %d",
               com_config->bus_instance, result);
    sns_spi_release_clk_resources(com_config->qup_type,
                                  com_config->qup_instance,
                                  (spi_instance_t)(com_config->bus_instance));
    sns_free(bus_info->bus_config);
    bus_info->bus_config = NULL;
    return SNS_RC_FAILED;
  }

  result =
      spi_enable_slave_index(spi_info->spi_handle, com_config->slave_control);
  if(SPI_ERROR(result))
  {
    SNS_PRINTF(
        ERROR, sns_fw_printf,
        "spi_enable_slave_index failure. spi_instance %d slave index %d err %d",
        com_config->bus_instance, com_config->slave_control, result);
    result = spi_close(spi_info->spi_handle);
    sns_spi_release_clk_resources(com_config->qup_type,
                                  com_config->qup_instance,
                                  (spi_instance_t)(com_config->bus_instance));
    sns_free(bus_info->bus_config);
    bus_info->bus_config = NULL;
    return SNS_RC_FAILED;
  }

  result = spi_power_on(spi_info->spi_handle);
  if(SPI_ERROR(result))
  {
    SNS_PRINTF(ERROR, sns_fw_printf,
               "spi_power_on failure. spi_instance %d err %d",
               com_config->bus_instance, result);
    result = spi_close(spi_info->spi_handle);
    sns_spi_release_clk_resources(com_config->qup_type,
                                  com_config->qup_instance,
                                  (spi_instance_t)(com_config->bus_instance));
    sns_free(bus_info->bus_config);
    bus_info->bus_config = NULL;
    return SNS_RC_FAILED;
  }

  SNS_PRINTF(LOW, sns_fw_printf,
             "spi_open success. hndl:%p instance:%u type:%u addr:0x%x",
             spi_info->spi_handle, com_config->bus_instance,
             com_config->bus_type, com_config->slave_control);

  return SNS_RC_SUCCESS;
}

/**--------------------------------------------------------------------------*/
SNS_SECTION(".text.sns")
sns_rc sns_open_spi(sns_sync_com_port_handle *port_handle)
{
  sns_com_port_priv_handle *priv_handle =
      (sns_com_port_priv_handle *)port_handle;
  sns_com_port_config *com_config = &priv_handle->com_config;

  if(com_config->bus_instance <= 0 ||
     com_config->bus_instance > SPI_INSTANCE_MAX)
  {
    return SNS_RC_INVALID_VALUE;
  }

  /* Check if the Major QUP type is not TOP and
     instance exceeds the count */
  if(com_config->qup_type != SNS_TOP_QUP &&
     (SNS_GET_QUP_INDEX(com_config->qup_instance) >
          SNS_MAX_SSC_QUP_INSTANCE_VALUE ||
      SNS_GET_QUP_INDEX(com_config->qup_instance) <
          SNS_MIN_SSC_QUP_INSTANCE_VALUE))
  {
    SNS_PRINTF(HIGH, sns_fw_printf,
               " Defaulting QUP type to SSC and instance %d, Provided "
               "q(typ/ins)(%d/%d)",
               SNS_DEFAULT_SSC_QUP_INSTANCE, com_config->qup_type,
               com_config->qup_instance);
    com_config->qup_type = SNS_SSC_QUP;
    com_config->qup_instance = SNS_DEFAULT_SSC_QUP_INSTANCE;
  }

  SNS_ISLAND_EXIT(); // spi_open must be called in big image

  return sns_open_spi_internal(port_handle);
}

/**--------------------------------------------------------------------------*/
static sns_rc __attribute__((noinline))
sns_close_spi_internal(sns_sync_com_port_handle *port_handle)
{
  sns_com_port_priv_handle *priv_handle =
      (sns_com_port_priv_handle *)port_handle;
  sns_com_port_config *com_config = &priv_handle->com_config;
  sns_bus_info *bus_info = &priv_handle->bus_info;
  sns_spi_info *spi_info = (sns_spi_info *)bus_info->bus_config;
  spi_status_t result;
  sns_rc rv = SNS_RC_SUCCESS;

  // TODO: handle closing SPI with transaction outstanding.
  result = spi_close(spi_info->spi_handle);
  if(SPI_ERROR(result))
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "spi_close failure. err %d", result);
    rv = SNS_RC_FAILED;
  }

  if(rv == SNS_RC_SUCCESS)
  {
    rv = sns_spi_release_clk_resources(
        com_config->qup_type, com_config->qup_instance,
        (spi_instance_t)(com_config->bus_instance));
    sns_free(bus_info->bus_config);
    bus_info->bus_config = NULL;
  }
  if(NULL != priv_handle->xfer_map.xfer_buffer_ptr)
  {
    sns_free(priv_handle->xfer_map.xfer_buffer_ptr);
    priv_handle->xfer_map.xfer_buffer_ptr = NULL;
  }
  priv_handle->xfer_map.xfer_buffer_len = 0;

  return rv;
}

/**--------------------------------------------------------------------------*/
SNS_SECTION(".text.sns")
sns_rc sns_close_spi(sns_sync_com_port_handle *port_handle)
{
  SNS_ISLAND_EXIT(); // spi_close must be called in big image
  return sns_close_spi_internal(port_handle);
}
/**--------------------------------------------------------------------------*/
SNS_SECTION(".text.sns")
sns_rc sns_pwr_update_spi(sns_sync_com_port_handle *port_handle,
                          bool power_bus_on)
{
  sns_rc rc = SNS_RC_SUCCESS;
  sns_com_port_priv_handle *priv_handle =
      (sns_com_port_priv_handle *)port_handle;
  sns_bus_info *bus_info = &priv_handle->bus_info;
  sns_spi_info *spi_info = (sns_spi_info *)bus_info->bus_config;
  spi_status_t status = SPI_SUCCESS;

  // If the SPI config was allocated in DDR, then vote to exit island mode
  if(!sns_island_is_island_ptr((intptr_t)spi_info) ||
     priv_handle->com_config.qup_type == SNS_TOP_QUP ||
     SNS_DDR_FOR_BUS_TRANSACTION)
  {
    SNS_ISLAND_EXIT();
  }

  if(power_bus_on)
  {
    status = spi_power_on(spi_info->spi_handle);
  }
  else
  {
    bool xfer_in_progress;
    xfer_in_progress = atomic_load(&spi_info->xfer_in_progress);
    if(xfer_in_progress)
    {
      sns_time one_ms = sns_convert_ns_to_ticks(1000000);
      SNS_PRINTF(MED, sns_fw_printf,
                 "Waiting for SPI txn complete before powering off...");
      while(xfer_in_progress)
      {
        sns_busy_wait(one_ms);
        xfer_in_progress = atomic_load(&spi_info->xfer_in_progress);
      }
      SNS_PRINTF(MED, sns_fw_printf, "Done waiting");
    }
    status = spi_power_off(spi_info->spi_handle);
  }

  if(!SPI_SUCCESS(status))
  {
    SNS_ISLAND_EXIT();
    SNS_PRINTF(ERROR, sns_fw_printf, "spi_pwr failure: on:%d status: %d",
               power_bus_on, status);
    rc = SNS_RC_FAILED;
  }

  return rc;
}

/**--------------------------------------------------------------------------*/
SNS_SECTION(".text.sns")
sns_rc sns_get_write_time_spi(sns_sync_com_port_handle *port_handle,
                              sns_time *write_time)
{
  sns_com_port_priv_handle *priv_handle =
      (sns_com_port_priv_handle *)port_handle;
  sns_bus_info *bus_info = &priv_handle->bus_info;
  sns_spi_info *spi_info = (sns_spi_info *)bus_info->bus_config;
  uint64_t bus_timestamp = 0;
  spi_status_t status;
  sns_spi_ts_type ts_type = SNS_SPI_TS_32;

  // If the SPI config was allocated in DDR, then vote to exit island mode
  if(!sns_island_is_island_ptr((intptr_t)spi_info) ||
     priv_handle->com_config.qup_type == SNS_TOP_QUP ||
     SNS_DDR_FOR_BUS_TRANSACTION)
  {
    SNS_ISLAND_EXIT();
  }

  status = spi_get_timestamp_64(spi_info->spi_handle,
                                (spi_timestamp_type *)&ts_type, &bus_timestamp);

  if(ts_type == SNS_SPI_TS_32)
  {
    bus_timestamp = (spi_info->txn_start_time & (UINT64_MAX << 32 << 7)) |
                    (bus_timestamp << 7);
    if(bus_timestamp < spi_info->txn_start_time)
    {
      bus_timestamp += 1ULL << 32 << 7;
    }
  }

  if(write_time != NULL)
  {
    *write_time = bus_timestamp;
  }

  return SPI_SUCCESS(status);
}

/**--------------------------------------------------------------------------*/
SNS_SECTION(".text.sns")
sns_rc sns_simple_txfr_port_spi(sns_sync_com_port_handle *port_handle,
                                bool is_write, bool save_write_time,
                                uint8_t *buffer, uint32_t bytes,
                                uint32_t *xfer_bytes)
{
  sns_rc return_code = SNS_RC_SUCCESS;
  sns_com_port_priv_handle *priv_handle =
      (sns_com_port_priv_handle *)port_handle;
  sns_bus_info *bus_info = &priv_handle->bus_info;
  sns_spi_info *spi_info = (sns_spi_info *)bus_info->bus_config;
  sns_com_port_config *com_config = &priv_handle->com_config;
  spi_status_t status;
  uint8_t *xfer_buffer_ptr = priv_handle->xfer_map.xfer_buffer_ptr;
  uint8_t *buffer_ptr;
  size_t xfer_buffer_len = priv_handle->xfer_map.xfer_buffer_len;

  // If the SPI config was allocated in DDR, then vote to exit island mode
  if(!sns_island_is_island_ptr((intptr_t)spi_info) ||
     priv_handle->com_config.qup_type == SNS_TOP_QUP ||
     SNS_DDR_FOR_BUS_TRANSACTION)
  {
    SNS_ISLAND_EXIT();
  }

  if(com_config->qup_type == SNS_TOP_QUP || SNS_DDR_FOR_BUS_TRANSACTION)
  {
    SNS_ISLAND_EXIT();
    if(xfer_buffer_len < SNS_GET_CACHE_ALIGNED_SIZE(bytes))
    {
      sns_free(xfer_buffer_ptr);
      xfer_buffer_ptr =
          sns_malloc(SNS_HEAP_MAIN, SNS_GET_CACHE_ALIGNED_SIZE(bytes));
      priv_handle->xfer_map.xfer_buffer_ptr = xfer_buffer_ptr;
      priv_handle->xfer_map.xfer_buffer_len = SNS_GET_CACHE_ALIGNED_SIZE(bytes);
    }
    buffer_ptr = SNS_GET_CACHE_ALIGNED_PTR(xfer_buffer_ptr);
  }
  else
  {
    if(xfer_buffer_len < SNS_GET_CACHE_ALIGNED_SIZE(bytes))
    {
      sns_free(xfer_buffer_ptr);
      xfer_buffer_ptr = sns_malloc(SNS_HEAP_PRAM, bytes);
      priv_handle->xfer_map.xfer_buffer_ptr = xfer_buffer_ptr;
      priv_handle->xfer_map.xfer_buffer_len = bytes;
    }
    buffer_ptr = xfer_buffer_ptr;
  }
  spi_descriptor_t descriptor = {// TODO: Put buffer in PRAM
                                 .tx_buf = is_write ? buffer_ptr : NULL,
                                 .rx_buf = is_write ? NULL : buffer_ptr,
                                 .len = bytes};

  spi_config_t config = {
      .spi_mode = SPI_MODE_0,
      .spi_cs_polarity = SPI_CS_ACTIVE_LOW,
      .endianness = SPI_NATIVE,
      .spi_bits_per_word = 8,
      .spi_slave_index = com_config->slave_control,
      .clk_freq_hz = com_config->max_bus_speed_KHz * 1000,
      .cs_clk_delay_cycles = 1,
      .inter_word_delay_cycles = 0,
      .cs_toggle = 0,
      .loopback_mode = 0,
  };

  if(xfer_buffer_ptr == NULL)
  {
    priv_handle->xfer_map.xfer_buffer_len = 0;
    return SNS_RC_NOT_AVAILABLE;
  }

  // For write transfers, copy the write data into PRAM for the transfer
  if(is_write)
  {
    if(!sns_island_is_island_ptr((intptr_t)buffer))
    {
      SNS_ISLAND_EXIT();
    }
    sns_memscpy(buffer_ptr, bytes, buffer, bytes);
  }

  status = spi_full_duplex(spi_info->spi_handle, &config, &descriptor, 1, NULL,
                           0, save_write_time);

  if(SPI_ERROR(status))
  {
    return_code = SNS_RC_FAILED;
  }
  *xfer_bytes = bytes;

  if(!is_write)
  {
    // For read operations, copy data from PRAM back to client buffer
    if(!sns_island_is_island_ptr((intptr_t)buffer))
    {
      SNS_ISLAND_EXIT();
    }
    sns_memscpy(buffer, bytes, buffer_ptr, bytes);
  }

  return return_code;
}

/**--------------------------------------------------------------------------*/
SNS_SECTION(".text.sns")
sns_rc
sns_txfr_port_spi(sns_sync_com_port_handle *port_handle,
                  uint8_t *read_buffer, // NULL allowed for write operation
                  uint32_t read_len_bytes, const uint8_t *write_buffer,
                  uint32_t write_len_bytes, uint8_t bits_per_word)
{
  sns_rc return_code = SNS_RC_SUCCESS;
  sns_com_port_priv_handle *priv_handle =
      (sns_com_port_priv_handle *)port_handle;
  sns_bus_info *bus_info = &priv_handle->bus_info;
  sns_spi_info *spi_info = (sns_spi_info *)bus_info->bus_config;
  sns_com_port_config *com_config = &priv_handle->com_config;
  spi_status_t status;
  uint32_t txn_len = SNS_MAX(read_len_bytes, write_len_bytes);

  // If the SPI config was allocated in DDR, then vote to exit island mode
  if(!sns_island_is_island_ptr((intptr_t)spi_info))
  {
    SNS_ISLAND_EXIT();
  }

  spi_descriptor_t descriptor = {
      .tx_buf = (uint8_t *)write_buffer,
      .rx_buf = read_buffer,
      .len = txn_len,
  };

  spi_config_t config = {
      .spi_mode = SPI_MODE_0,
      .spi_cs_polarity = SPI_CS_ACTIVE_LOW,
      .endianness = SPI_NATIVE,
      .spi_bits_per_word = bits_per_word,
      .spi_slave_index = com_config->slave_control,
      .clk_freq_hz = com_config->max_bus_speed_KHz * 1000,
      .cs_clk_delay_cycles = 1,
      .inter_word_delay_cycles = 0,
      .cs_toggle = 0,
      .loopback_mode = 0,
  };

  // For write transfers, copy the write data into PRAM for the transfer
  if(!sns_island_is_island_ptr((intptr_t)write_buffer) ||
     !sns_island_is_island_ptr((intptr_t)read_buffer))
  {
    SNS_ISLAND_EXIT();
  }

  status = spi_full_duplex(spi_info->spi_handle, &config, &descriptor, 1, NULL,
                           0, FALSE);
  if(SPI_ERROR(status))
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "spi_full_duplex failure. err %d", status);
    return_code = SNS_RC_FAILED;
  }

  return return_code;
}

/**--------------------------------------------------------------------------*/
SNS_SECTION(".text.sns")
sns_rc sns_vectored_rw_spi_ex(sns_sync_com_port_handle *port_handle,
                              sns_com_port_config_ex *com_port_ex,
                              sns_port_vector *vectors, int32_t num_vectors,
                              bool save_write_time, uint32_t *xfer_bytes,
                              bool async)
{
  sns_rc return_code = SNS_RC_FAILED;
  sns_com_port_priv_handle *priv_handle =
      (sns_com_port_priv_handle *)port_handle;
  sns_bus_info *bus_info = &priv_handle->bus_info;
  sns_spi_info *spi_info = (sns_spi_info *)bus_info->bus_config;
  sns_com_port_config *com_config = &priv_handle->com_config;
  sns_com_port_vector_xfer_map *xfer_map = &priv_handle->xfer_map;
  int i, j;
  int register_len;
  spi_status_t status = SPI_ERROR_INVALID_PARAM;
  spi_descriptor_t spi_desc[MAX_SPI_DESCRIPTORS];
  int num_bytes = 0;
  int total_memory_length = 0;
  int memory_length_offset = 0;
  uint32_t bit_shift_width = 0xFF;
  uint8_t *buffer_ptr = NULL;
  /* Since Utils can be dynamically linked and sns_com_port_config_ex
   * can be updated with new elements in future, we need to make sure the
   * sns_com_port_config_ex structure passed has at least the elements
   * this function expects. */
  if(com_port_ex->size_of_this_struct < sizeof(sns_com_port_config_ex))
  {
    SNS_PRINTF(ERROR, sns_fw_printf,
               "input structure sns_com_port_config_ex is not compatible");
    return return_code;
  }

  spi_config_t slave_config = {
      .spi_mode = (uint8_t)com_port_ex->bus_mode,
      .spi_cs_polarity = SPI_CS_ACTIVE_LOW,
      .endianness = SPI_NATIVE,
      .spi_bits_per_word = 8,
      .spi_slave_index = com_config->slave_control,
      .clk_freq_hz = com_config->max_bus_speed_KHz * 1000,
      .cs_clk_delay_cycles = 1,
      .inter_word_delay_cycles = 0,
      .cs_toggle = 0,
      .loopback_mode = 0,
  };

  // If the SPI config was allocated in DDR, then vote to exit island mode
  if(!sns_island_is_island_ptr((intptr_t)spi_info))
  {
    SNS_ISLAND_EXIT();
  }

  switch(com_config->reg_addr_type)
  {
  case SNS_REG_ADDR_16_BIT:
    register_len = 2;
    bit_shift_width = 0xFFFF;
    break;
  case SNS_REG_ADDR_32_BIT:
    register_len = 4;
    bit_shift_width = 0xFFFFFFFF;
    break;
  default:
  case SNS_REG_ADDR_8_BIT:
    register_len = 1;
    bit_shift_width = 0xFF;
    break;
  }

  xfer_map->orig_vector_ptr = vectors;
  xfer_map->num_vectors = num_vectors;
  for(i = 0; i < num_vectors; i++)
  {
    if(com_config->qup_type == SNS_TOP_QUP || SNS_DDR_FOR_BUS_TRANSACTION)
    {
      total_memory_length +=
          SNS_GET_CACHE_ALIGNED_SIZE(register_len + vectors[i].bytes);
    }
    else
    {
      total_memory_length += register_len + vectors[i].bytes;
    }
  }

  if(com_config->qup_type == SNS_TOP_QUP || SNS_DDR_FOR_BUS_TRANSACTION)
  {
    if(xfer_map->xfer_buffer_len <
       SNS_GET_CACHE_ALIGNED_SIZE(total_memory_length))
    {
      SNS_ISLAND_EXIT();
      sns_free(xfer_map->xfer_buffer_ptr);
      xfer_map->xfer_buffer_ptr = sns_malloc(
          SNS_HEAP_MAIN, SNS_GET_CACHE_ALIGNED_SIZE(total_memory_length));
      xfer_map->xfer_buffer_len =
          SNS_GET_CACHE_ALIGNED_SIZE(total_memory_length);
    }
  }
  else
  {
    if(xfer_map->xfer_buffer_len < total_memory_length)
    {
      sns_free(xfer_map->xfer_buffer_ptr);
      xfer_map->xfer_buffer_ptr =
          sns_malloc(SNS_HEAP_PRAM, (total_memory_length));
      xfer_map->xfer_buffer_len = total_memory_length;
    }
  }

  for(i = 0, j = 0; i < num_vectors; i++, j++)
  {
    uint32_t temp_reg_addr = 0;

    if(j >= MAX_SPI_DESCRIPTORS)
    {
      sns_com_port_profile(port_handle, PROFILE_START, PROFILE_SPI_TRANSFER);
      status = spi_full_duplex(spi_info->spi_handle, &slave_config, spi_desc, j,
                               NULL, NULL, save_write_time);
      sns_com_port_profile(port_handle, PROFILE_STOP, PROFILE_SPI_TRANSFER);
      j = 0;
    }

    // Copy the register address into the buffer after modifying the read/write
    // bit
    if(com_port_ex->rw_mode == SNS_SPI_R_W_BIT_MODE_0)
    {
      if(vectors[i].is_write)
      {
        temp_reg_addr = vectors[i].reg_addr & (bit_shift_width >> 1);
      }
      else
      {
        temp_reg_addr = vectors[i].reg_addr | (~(bit_shift_width >> 1));
      }
    }
    else if(com_port_ex->rw_mode == SNS_SPI_R_W_BIT_MODE_1)
    {
      if(vectors[i].is_write)
      {
        temp_reg_addr = vectors[i].reg_addr | (~(bit_shift_width >> 1));
      }
      else
      {
        temp_reg_addr = vectors[i].reg_addr & (bit_shift_width >> 1);
      }
    }
    else if(com_port_ex->rw_mode == SNS_SPI_R_W_BIT_MODE_2)
    {
      temp_reg_addr = vectors[i].reg_addr;
    }

    /* Convert Address Byte order to Big Endian */
    if(com_port_ex->rw_mode != SNS_SPI_R_W_BIT_MODE_2)
    {
      switch(com_config->reg_addr_type)
      {
      case SNS_REG_ADDR_8_BIT:
        temp_reg_addr = temp_reg_addr & 0xFF;
        break;
      case SNS_REG_ADDR_16_BIT:
        temp_reg_addr =
            ((temp_reg_addr & 0x00FF) << 8) | ((temp_reg_addr & 0xFF00) >> 8);
        break;
      case SNS_REG_ADDR_32_BIT:
        temp_reg_addr = ((temp_reg_addr & 0x000000FF) << 24) |
                        ((temp_reg_addr & 0x0000FF00) << 8) |
                        ((temp_reg_addr & 0x00FF0000) >> 8) |
                        ((temp_reg_addr & 0xFF000000) >> 24);
        break;
      default:
        break;
      }
    }
    if(com_config->qup_type == SNS_TOP_QUP || SNS_DDR_FOR_BUS_TRANSACTION)
    {
      buffer_ptr = SNS_GET_CACHE_ALIGNED_PTR(xfer_map->xfer_buffer_ptr +
                                             memory_length_offset);
    }
    else
    {
      buffer_ptr = xfer_map->xfer_buffer_ptr + memory_length_offset;
    }
    memory_length_offset += register_len + vectors[i].bytes;
    sns_memscpy(buffer_ptr, register_len, (uint8_t *)&temp_reg_addr,
                register_len);

    // Update the SPI descriptor
    if(vectors[i].is_write)
    {
      if(!sns_island_is_island_ptr((intptr_t)vectors[i].buffer))
      {
        SNS_ISLAND_EXIT();
      }
      sns_memscpy(buffer_ptr + register_len, vectors[i].bytes,
                  vectors[i].buffer, vectors[i].bytes);
      spi_desc[j].tx_buf = buffer_ptr;
      spi_desc[j].rx_buf = NULL;
    }
    else
    {
      spi_desc[j].tx_buf = buffer_ptr;
      spi_desc[j].rx_buf = buffer_ptr;
    }
    spi_desc[j].len = vectors[i].bytes + register_len;
    spi_desc[j].leave_cs_asserted = false;
    num_bytes += vectors[i].bytes;
  }
  if(save_write_time)
  {
    spi_info->txn_start_time = sns_get_system_time();
  }

  if(async)
  {
    atomic_store(&spi_info->xfer_in_progress, true);
  }

  if(j > 0)
  {
    sns_com_port_profile(port_handle, PROFILE_START, PROFILE_SPI_TRANSFER);
    status =
        spi_full_duplex(spi_info->spi_handle, &slave_config, spi_desc, j,
                        async ? sns_spi_callback : NULL,
                        async ? (void *)port_handle : NULL, save_write_time);
    sns_com_port_profile(port_handle, PROFILE_STOP, PROFILE_SPI_TRANSFER);
  }

  if(SPI_SUCCESS(status))
  {
    if(xfer_bytes != NULL)
    {
      *xfer_bytes = num_bytes;
    }
    return_code = SNS_RC_SUCCESS;
  }
  else
  {
    SNS_ISLAND_EXIT();
    SNS_PRINTF(ERROR, sns_fw_printf, "spi_full_duplex failure. err %d", status);
    if(async)
    {
      atomic_store(&spi_info->xfer_in_progress, false);
    }
  }

  if(!async)
  {
    memory_length_offset = 0;
    for(i = 0; i < num_vectors; i++)
    {
      if(!vectors[i].is_write && SPI_SUCCESS(status))
      {
        // For read operations, copy data from PRAM back to client buffer
        if(!sns_island_is_island_ptr((intptr_t)vectors[i].buffer))
        {
          SNS_ISLAND_EXIT();
        }
        if(com_config->qup_type == SNS_TOP_QUP || SNS_DDR_FOR_BUS_TRANSACTION)
        {
          buffer_ptr = SNS_GET_CACHE_ALIGNED_PTR(xfer_map->xfer_buffer_ptr +
                                                 memory_length_offset);
        }
        else
        {
          buffer_ptr = xfer_map->xfer_buffer_ptr + memory_length_offset;
        }
        sns_memscpy(vectors[i].buffer, vectors[i].bytes,
                    buffer_ptr + register_len, vectors[i].bytes);
      }
      memory_length_offset += register_len + vectors[i].bytes;
    }
  }
  return return_code;
}
/**--------------------------------------------------------------------------*/
SNS_SECTION(".text.sns")
sns_rc sns_ascp_register_spi_callback(void *port_handle,
                                      sns_ascp_callback callback, void *args)
{
  sns_com_port_priv_handle *priv_handle =
      (sns_com_port_priv_handle *)port_handle;
  priv_handle->callback = callback;
  priv_handle->args = args;
  return SNS_RC_SUCCESS;
}
/**--------------------------------------------------------------------------*/
SNS_SECTION(".text.sns")
sns_rc sns_sync_vectored_rw_spi(sns_sync_com_port_handle *port_handle,
                                sns_port_vector *vectors, int32_t num_vectors,
                                bool save_write_time, uint32_t *xfer_bytes)
{
  return sns_vectored_rw_spi_ex(port_handle, &com_port_ex_default, vectors,
                                num_vectors, save_write_time, xfer_bytes,
                                false);
}

/**--------------------------------------------------------------------------*/
SNS_SECTION(".text.sns")
sns_rc sns_sync_vectored_rw_spi_ex(sns_sync_com_port_handle *port_handle,
                                   sns_com_port_config_ex *com_port_ex,
                                   sns_port_vector *vectors,
                                   int32_t num_vectors, bool save_write_time,
                                   uint32_t *xfer_bytes)
{
  return sns_vectored_rw_spi_ex(port_handle, com_port_ex, vectors, num_vectors,
                                save_write_time, xfer_bytes, false);
}

/**--------------------------------------------------------------------------*/
SNS_SECTION(".text.sns")
sns_rc sns_async_vectored_rw_spi(void *port_handle, uint8_t num_vectors,
                                 sns_port_vector vectors[],
                                 bool save_write_time)
{
  return sns_vectored_rw_spi_ex(port_handle, &com_port_ex_default, vectors,
                                num_vectors, save_write_time, NULL, true);
}

SNS_SECTION(".text.sns")
sns_rc sns_spi_setup_clk_resources(uint8_t qup_type, uint8_t qup_instance,
                                   uint8_t bus_instance,
                                   int32_t new_bus_speed_KHz,
                                   int32_t *prev_bus_speed_KHz)
{

  if(bus_instance < SNS_MIN_BUS_INSTANCE_VALUE)
    return SNS_RC_INVALID_VALUE;

  int i;
  sns_rc rc = SNS_RC_SUCCESS;
  spi_status_t result;
  sns_osa_lock_attr lock_attr;
  unsigned int expected = 0;

  if(atomic_compare_exchange_strong(&sns_spi_initialized, &expected, 1))
  {
    rc = sns_osa_lock_attr_init(&lock_attr);
    SNS_ASSERT(SNS_RC_SUCCESS == rc);
    rc = sns_osa_lock_init(&lock_attr, &sns_spi_clk_resource_lock);
    SNS_ASSERT(SNS_RC_SUCCESS == rc);
    atomic_store(&sns_spi_initialized, 2);
  }

  while(atomic_load(&sns_spi_initialized) != 2)

    if(new_bus_speed_KHz == 0)
    {
      return SNS_RC_FAILED;
    }
  sns_osa_lock_acquire(&sns_spi_clk_resource_lock);
  // Check to see if this instance already has a vote:
  for(i = 0; i < MAX_INSTANCES_SUPPORTED; i++)
  {
    if(bus_instance == sns_spi_clk_resources[i].bus_instance)
    {
      break;
    }
  }

  if(i == MAX_INSTANCES_SUPPORTED)
  {
    // No existing vote -- add a new one.
    for(i = 0; i < MAX_INSTANCES_SUPPORTED; i++)
    {
      if(sns_spi_clk_resources[i].bus_instance == 0)
      {
        sns_spi_clk_resources[i].bus_instance = bus_instance;
        break;
      }
    }
  }

  if(i < MAX_INSTANCES_SUPPORTED)
  {
    sns_spi_clk_resources[i].ref_count++;
    if(prev_bus_speed_KHz)
    {
      *prev_bus_speed_KHz = sns_spi_clk_resources[i].bus_speed_khz;
    }

    if(sns_spi_clk_resources[i].bus_speed_khz != new_bus_speed_KHz)
    {
      SNS_ISLAND_EXIT(); // setup_lpi_resource must be called in big image

      result = spi_setup_island_resource_ex(
          SNS_GET_QUP_TYPE(qup_type, qup_instance),
          SNS_SPI_GET_QUP_SE_INDEX(bus_instance), new_bus_speed_KHz * 1000);

      if(SPI_ERROR(result))
      {
        SNS_PRINTF(ERROR, sns_fw_printf,
                   "spi_setup_island_resource failure. spi_instance %d err %d",
                   bus_instance, result);
        rc = SNS_RC_FAILED;
      }
      else if(sns_spi_clk_resources[i].bus_speed_khz != 0)
      {
        // Keep the SPI bus driver reference count at 1.
        spi_reset_island_resource_ex(SNS_GET_QUP_TYPE(qup_type, qup_instance),
                                     SNS_SPI_GET_QUP_SE_INDEX(bus_instance),
                                     NULL);
      }
      sns_spi_clk_resources[i].bus_speed_khz = new_bus_speed_KHz;
    }
  }
  else
  {
    rc = SNS_RC_FAILED;
  }
  sns_osa_lock_release(&sns_spi_clk_resource_lock);
  return rc;
}

SNS_SECTION(".text.sns")
sns_rc sns_spi_release_clk_resources(uint8_t qup_type, uint8_t qup_instance,
                                     uint8_t bus_instance)
{
  if(bus_instance < SNS_MIN_BUS_INSTANCE_VALUE)
    return SNS_RC_INVALID_VALUE;

  int i;
  sns_rc rc = SNS_RC_FAILED;
  spi_status_t result;

  sns_osa_lock_acquire(&sns_spi_clk_resource_lock);
  // Check to see if this instance has a vote:
  for(i = 0; i < MAX_INSTANCES_SUPPORTED; i++)
  {
    if(bus_instance == sns_spi_clk_resources[i].bus_instance)
    {
      break;
    }
  }
  if(i < MAX_INSTANCES_SUPPORTED)
  {
    if(sns_spi_clk_resources[i].ref_count == 1)
    {
      SNS_ISLAND_EXIT(); // reset_lpi_resource must be called in big image
      sns_spi_clk_resources[i].bus_speed_khz = 0;

      result = spi_reset_island_resource_ex(
          SNS_GET_QUP_TYPE(qup_type, qup_instance),
          SNS_SPI_GET_QUP_SE_INDEX(bus_instance), NULL);
      if(SPI_SUCCESS(result))
      {
        rc = SNS_RC_SUCCESS;
      }
      else
      {
        SNS_PRINTF(ERROR, sns_fw_printf,
                   "spi_reset_island_resource failure. instance %d err %d",
                   bus_instance, result);
      }
      sns_spi_clk_resources[i].ref_count = 0;
    }
    else if(sns_spi_clk_resources[i].ref_count > 0)
    {
      sns_spi_clk_resources[i].ref_count--;
      rc = SNS_RC_SUCCESS;
    }
  }
  sns_osa_lock_release(&sns_spi_clk_resource_lock);
  return rc;
}

sns_sync_com_port_service_api spi_port_api SNS_SECTION(".data.sns") = {
    .struct_len = sizeof(sns_sync_com_port_service_api),
    .sns_scp_register_com_port = NULL,
    .sns_scp_deregister_com_port = NULL,
    .sns_scp_open = &sns_open_spi,
    .sns_scp_close = &sns_close_spi,
    .sns_scp_update_bus_power = &sns_pwr_update_spi,
    .sns_scp_register_rw = &sns_sync_vectored_rw_spi,
    .sns_scp_register_rw_ex = &sns_sync_vectored_rw_spi_ex,
    .sns_scp_simple_rw = &sns_simple_txfr_port_spi,
    .sns_scp_get_write_time = &sns_get_write_time_spi,
    .sns_scp_get_dynamic_addr = NULL,
    .sns_scp_get_version = NULL,
    .sns_scp_issue_ccc = NULL,
    .sns_scp_rw = &sns_txfr_port_spi,
};
/**--------------------------------------------------------------------------*/
sns_ascp_port_api spi_async_port_api SNS_SECTION(".data.sns") = {
    .struct_len = sizeof(sns_ascp_port_api),
    .sync_com_port = &spi_port_api,
    .sns_ascp_register_callback = &sns_ascp_register_spi_callback,
    .sns_ascp_register_rw = &sns_async_vectored_rw_spi,
};

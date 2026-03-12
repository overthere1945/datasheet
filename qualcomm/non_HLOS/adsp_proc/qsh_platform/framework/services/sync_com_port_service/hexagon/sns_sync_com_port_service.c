/**
 * sns_sync_com_port_service.c
 *
 * The Synchronous COM Port Service implementation
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

/*==============================================================================
  Include Files
  ============================================================================*/

#include "sns_assert.h"
#include "sns_com_port_priv.h"
#include "sns_com_port_profile.h"
#include "sns_com_port_types.h"
#include "sns_island.h"
#include "sns_island_util.h"
#include "sns_mem_util.h"
#include "sns_memmgr.h"
#include "sns_osa_lock.h"
#include "sns_printf_int.h"
#include "sns_sync_com_port_service.h"

/*==============================================================================
  Macros
  ============================================================================*/

#ifndef SNS_MAX_COM_PORTS
#define SNS_MAX_COM_PORTS 16
#endif

/*
 * Proxy com ports are used to put proxy bus power ON/OFF vote when Q6 sensor
 * Wakeup rate is greater than SNS_HIGH_PERF_MODE_WAKEUP_RATE_THRESHOLD.
 * One Proxy com port is added per bus instance.
 */
#define SNS_MAX_PROXY_COM_PORTS 8

#define SNS_HIGH_PERF_MODE_WAKEUP_RATE_THRESHOLD 400

/*==============================================================================
  Static Function Declarations
  ============================================================================*/

static bool save_proxy_com_port(sns_com_port_priv_handle *port_handle);
static bool remove_proxy_com_port(sns_com_port_priv_handle *port_handle);

/*==============================================================================
  Static Data Definitions
  ============================================================================*/

static sns_com_port_priv_handle *
    sns_com_ports[SNS_MAX_COM_PORTS] SNS_SECTION(".data.sns") = {0};
static sns_osa_lock com_port_list_lock SNS_SECTION(".data.sns");

static sns_com_port_proxy_handle
    sns_proxy_com_ports[SNS_MAX_PROXY_COM_PORTS] SNS_SECTION(".data.sns") = {0};
static sns_osa_lock proxy_com_port_lock SNS_SECTION(".data.sns");

/** Version:
 */
static sns_sync_com_port_version scp_version SNS_SECTION(".data.sns") = {
    .major = 1, .minor = 1};

static sns_sync_com_port_service sync_com_port_service SNS_SECTION(".data.sns");
/**
 * Private state definition.
 */
/*
struct sns_fw_sync_com_port_service
{
  sns_sync_com_port_service service;
} ;
*/
/** Vtables for bus specific API.
 */
extern sns_sync_com_port_service_api i2c_port_api;
extern sns_sync_com_port_service_api spi_port_api;
extern sns_sync_com_port_service_api remote_port_api;

/** Interface functions for supported bus ports.
 *
 *  IMPORTANT: This list must match enum definitions in
 *  sns_bus_type.
 */
sns_sync_com_port_service_api *scp_port_apis[] SNS_SECTION(".data.sns") = {
    &i2c_port_api, // SNS_BUS_I2C
    &spi_port_api, // SNS_BUS_SPI
    NULL,          // placeholder for SNS_BUS_UART &uart_port,
    &i2c_port_api, // SNS_BUS_I3C_SDR
    &i2c_port_api, // SNS_BUS_I3C_DDR
    &i2c_port_api, // SNS_BUS_I2C_I3C_LEGACY
    &remote_port_api,
};

/*==============================================================================
  Static Function Definitions
  ============================================================================*/

SNS_SECTION(".text.sns")
static bool save_com_port(sns_com_port_priv_handle *port_handle)
{
  bool found = false;

  save_proxy_com_port(port_handle);

  sns_osa_lock_acquire(&com_port_list_lock);
  for(uint8_t i = 0; i < ARR_SIZE(sns_com_ports); i++)
  {
    if(sns_com_ports[i] == NULL)
    {
      found = true;
      sns_com_ports[i] = port_handle;
      break;
    }
  }
  if(!found)
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "save: 0x%X not saved", port_handle);
  }
  sns_osa_lock_release(&com_port_list_lock);
  return found;
}
/*----------------------------------------------------------------------------*/

SNS_SECTION(".text.sns")
static bool remove_com_port(sns_com_port_priv_handle *port_handle)
{
  bool found = false;

  remove_proxy_com_port(port_handle);

  sns_osa_lock_acquire(&com_port_list_lock);
  for(uint8_t i = 0; i < ARR_SIZE(sns_com_ports); i++)
  {
    if(sns_com_ports[i] == port_handle)
    {
      found = true;
      sns_com_ports[i] = NULL;
      break;
    }
  }
  if(!found)
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "remove: 0x%X not found", port_handle);
  }
  sns_osa_lock_release(&com_port_list_lock);
  return found;
}
/*==============================================================================
  Public Function Definitions
  ============================================================================*/

SNS_SECTION(".text.sns")
sns_rc sns_scp_open(sns_sync_com_port_handle *port_handle)
{
  if(port_handle == NULL)
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "open: null port_handle");
    return SNS_RC_INVALID_VALUE;
  }
  // If the SCP state was allocated in DDR, then vote to exit island mode
  if(!sns_island_is_island_ptr((intptr_t)port_handle))
  {
    SNS_ISLAND_EXIT();
  }
  sns_com_port_priv_handle *priv_handle =
      (sns_com_port_priv_handle *)port_handle;
  sns_bus_info *bus_info = &priv_handle->bus_info;
  sns_rc return_code = SNS_RC_SUCCESS;

  if(bus_info->bus_type < SNS_BUS_MIN || bus_info->bus_type > SNS_BUS_MAX)
  {
    return SNS_RC_INVALID_VALUE;
  }

  if(bus_info->opened == true)
  {
    return SNS_RC_SUCCESS;
  }

  return_code = scp_port_apis[bus_info->bus_type]->sns_scp_open(port_handle);

  if(return_code != SNS_RC_SUCCESS)
  {
    bus_info->opened = false;
  }
  else
  {
    sns_com_port_profile_init(port_handle);
    bus_info->opened = true;
    bus_info->power_on = true;
    COM_PORT_PRINTF(HIGH, sns_fw_printf,
                    "Port handle %p, bus type: %d: with power status %d",
                    port_handle, bus_info->bus_type, bus_info->power_on);
  }

  return return_code;
}
/*----------------------------------------------------------------------------*/

SNS_SECTION(".text.sns")
sns_rc sns_scp_update_bus_power(sns_sync_com_port_handle *port_handle,
                                bool power_bus_on)
{
  if(port_handle == NULL)
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "power: null port_handle");
    return SNS_RC_INVALID_VALUE;
  }
  // If the SCP state was allocated in DDR, then vote to exit island mode
  if(!sns_island_is_island_ptr((intptr_t)port_handle))
  {
    SNS_ISLAND_EXIT();
  }
  sns_com_port_priv_handle *priv_handle =
      (sns_com_port_priv_handle *)port_handle;
  sns_bus_info *bus_info = &priv_handle->bus_info;
  sns_rc return_code = SNS_RC_FAILED;

  if(bus_info->opened == true)
  {
    if(bus_info->power_on != power_bus_on)
    {
      return_code = scp_port_apis[bus_info->bus_type]->sns_scp_update_bus_power(
          port_handle, power_bus_on);
      if(return_code == SNS_RC_SUCCESS)
      {
        bus_info->power_on = power_bus_on;
        COM_PORT_PRINTF(HIGH, sns_fw_printf,
                        "scp_update_bus_power: Port handle %p, bus type: %d: "
                        "with power status %d",
                        port_handle, bus_info->bus_type, power_bus_on);
      }
    }
    else
    {
      // Bus already in selected power state
      return_code = SNS_RC_SUCCESS;
    }
  }
  return return_code;
}
/*----------------------------------------------------------------------------*/

SNS_SECTION(".text.sns")
sns_rc sns_scp_close(sns_sync_com_port_handle *port_handle)
{
  if(port_handle == NULL)
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "close: null port_handle");
    return SNS_RC_INVALID_VALUE;
  }
  // If the SCP state was allocated in DDR, then vote to exit island mode
  if(!sns_island_is_island_ptr((intptr_t)port_handle))
  {
    SNS_ISLAND_EXIT();
  }
  sns_com_port_priv_handle *priv_handle =
      (sns_com_port_priv_handle *)port_handle;
  sns_bus_info *bus_info = &priv_handle->bus_info;
  sns_rc return_code = SNS_RC_SUCCESS;

  if(bus_info->opened == true)
  {
    sns_scp_update_bus_power(port_handle, false);
    return_code = scp_port_apis[bus_info->bus_type]->sns_scp_close(port_handle);
    sns_com_port_latency_dump(port_handle);
    sns_com_port_profile_deinit(port_handle);
    bus_info->opened = false;

    COM_PORT_PRINTF(HIGH, sns_fw_printf, "sns_scp_close: Port handle %p",
                    port_handle);
  }

  return return_code;
}
/*----------------------------------------------------------------------------*/

SNS_SECTION(".text.sns")
sns_rc sns_scp_register_rw(sns_sync_com_port_handle *port_handle,
                           sns_port_vector *vectors, int32_t num_vectors,
                           bool save_write_time, uint32_t *xfer_bytes)
{
  if(port_handle == NULL)
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "rw: null port_handle");
    return SNS_RC_INVALID_VALUE;
  }
  // If the SCP state was allocated in DDR, then vote to exit island mode
  if(!sns_island_is_island_ptr((intptr_t)port_handle))
  {
    SNS_ISLAND_EXIT();
  }
  sns_com_port_priv_handle *priv_handle =
      (sns_com_port_priv_handle *)port_handle;
  sns_bus_info *bus_info = &priv_handle->bus_info;
  sns_rc return_code = SNS_RC_FAILED;

  if(bus_info->opened == true && bus_info->power_on == true)
  {
    return_code = scp_port_apis[bus_info->bus_type]->sns_scp_register_rw(
        port_handle, vectors, num_vectors, save_write_time, xfer_bytes);
  }

  return return_code;
}
/*----------------------------------------------------------------------------*/

SNS_SECTION(".text.sns")
sns_rc sns_scp_register_rw_ex(sns_sync_com_port_handle *port_handle,
                              sns_com_port_config_ex *com_port_ex,
                              sns_port_vector *vectors, int32_t num_vectors,
                              bool save_write_time, uint32_t *xfer_bytes)
{
  if(port_handle == NULL)
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "rw_ex: null port_handle");
    return SNS_RC_INVALID_VALUE;
  }
  // If the SCP state was allocated in DDR, then vote to exit island mode
  if(!sns_island_is_island_ptr((intptr_t)port_handle))
  {
    SNS_ISLAND_EXIT();
  }
  sns_com_port_priv_handle *priv_handle =
      (sns_com_port_priv_handle *)port_handle;
  sns_bus_info *bus_info = &priv_handle->bus_info;
  sns_rc return_code = SNS_RC_FAILED;

  if(scp_port_apis[bus_info->bus_type]->sns_scp_register_rw_ex == NULL)
  {
    return SNS_RC_NOT_AVAILABLE;
  }

  if(bus_info->opened == true && bus_info->power_on == true)
  {
    return_code = scp_port_apis[bus_info->bus_type]->sns_scp_register_rw_ex(
        port_handle, com_port_ex, vectors, num_vectors, save_write_time,
        xfer_bytes);
  }

  return return_code;
}
/*----------------------------------------------------------------------------*/

SNS_SECTION(".text.sns")
sns_rc sns_scp_simple_rw(sns_sync_com_port_handle *port_handle, bool is_write,
                         bool save_write_time, uint8_t *buffer, uint32_t bytes,
                         uint32_t *xfer_bytes)
{
  if(port_handle == NULL)
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "simple_rw: null port_handle");
    return SNS_RC_INVALID_VALUE;
  }
  // If the SCP state was allocated in DDR, then vote to exit island mode
  if(!sns_island_is_island_ptr((intptr_t)port_handle))
  {
    SNS_ISLAND_EXIT();
  }
  sns_com_port_priv_handle *priv_handle =
      (sns_com_port_priv_handle *)port_handle;
  sns_bus_info *bus_info = &priv_handle->bus_info;
  sns_rc return_code = SNS_RC_FAILED;

  if(bus_info->opened == true && bus_info->power_on == true)
  {
    return_code = scp_port_apis[bus_info->bus_type]->sns_scp_simple_rw(
        port_handle, is_write, save_write_time, buffer, bytes, xfer_bytes);
  }
  return return_code;
}
/*----------------------------------------------------------------------------*/

SNS_SECTION(".text.sns")
sns_rc sns_scp_rw(sns_sync_com_port_handle *port_handle,
                  uint8_t *read_buffer, // NULL allowed for write operation
                  uint32_t read_len, const uint8_t *write_buffer,
                  uint32_t write_len, uint8_t bits_per_word)
{
  sns_com_port_priv_handle *priv_handle =
      (sns_com_port_priv_handle *)port_handle;
  sns_bus_info *bus_info = &priv_handle->bus_info;
  sns_rc return_code = SNS_RC_FAILED;

  if(port_handle == NULL)
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "null port_handle");
    return SNS_RC_INVALID_VALUE;
  }

  // If the SCP state was allocated in DDR, then vote to exit island mode
  if(!sns_island_is_island_ptr((intptr_t)port_handle))
  {
    SNS_ISLAND_EXIT();
  }

  if(scp_port_apis[bus_info->bus_type]->sns_scp_rw == NULL)
  {
    return SNS_RC_NOT_AVAILABLE;
  }

  if(bus_info->opened == true && bus_info->power_on == true)
  {
    return_code = scp_port_apis[bus_info->bus_type]->sns_scp_rw(
        port_handle, read_buffer, read_len, write_buffer, write_len,
        bits_per_word);
  }

  return return_code;
}
/*----------------------------------------------------------------------------*/

SNS_SECTION(".text.sns")
sns_rc sns_scp_get_write_time(sns_sync_com_port_handle *port_handle,
                              sns_time *write_time)
{
  if(port_handle == NULL)
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "get_write_time: null port_handle");
    return SNS_RC_INVALID_VALUE;
  }
  // If the SCP state was allocated in DDR, then vote to exit island mode
  if(!sns_island_is_island_ptr((intptr_t)port_handle))
  {
    SNS_ISLAND_EXIT();
  }
  sns_com_port_priv_handle *priv_handle =
      (sns_com_port_priv_handle *)port_handle;
  sns_bus_info *bus_info = &priv_handle->bus_info;
  sns_rc return_code = SNS_RC_FAILED;

  if(bus_info->opened == true && bus_info->power_on == true)
  {
    if(write_time != NULL)
    {
      return_code = scp_port_apis[bus_info->bus_type]->sns_scp_get_write_time(
          port_handle, write_time);
    }
  }
  return return_code;
}
/*----------------------------------------------------------------------------*/

SNS_SECTION(".text.sns")
sns_rc sns_scp_get_version(sns_sync_com_port_version *version)
{
  sns_rc return_code = SNS_RC_INVALID_VALUE;
  if(version != NULL)
  {
    sns_memscpy(version, sizeof(sns_sync_com_port_version), &scp_version,
                sizeof(sns_sync_com_port_version));
    return_code = SNS_RC_SUCCESS;
  }

  return return_code;
}
/*----------------------------------------------------------------------------*/

SNS_SECTION(".text.sns")
sns_rc sns_scp_deregister_com_port(sns_sync_com_port_handle **port_handle)
{
  sns_rc rc = SNS_RC_INVALID_VALUE;
  if((port_handle == NULL) || (*port_handle == NULL))
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "null port_handle");
    return SNS_RC_INVALID_VALUE;
  }

  // If the SCP state was allocated in DDR, then vote to exit island mode
  if(!sns_island_is_island_ptr((intptr_t)port_handle))
  {
    SNS_ISLAND_EXIT();
  }

  if(remove_com_port((sns_com_port_priv_handle *)*port_handle))
  {
    sns_scp_update_bus_power(*port_handle, false);
    sns_scp_close(*port_handle);
    sns_free(*port_handle);
    *port_handle = NULL;
    rc = SNS_RC_SUCCESS;
  }

  return rc;
}
/*----------------------------------------------------------------------------*/

SNS_SECTION(".text.sns")
sns_rc sns_scp_register_com_port(sns_com_port_config const *com_config,
                                 sns_sync_com_port_handle **port_handle)
{
  sns_com_port_priv_handle *port_priv_handle;

  if(com_config == NULL || port_handle == NULL ||
     (com_config->slave_control == 0 && com_config->bus_instance == 0))
  {
    return SNS_RC_INVALID_VALUE;
  }
  if(NULL != *port_handle)
  {
    sns_scp_deregister_com_port(port_handle);
  }

  port_priv_handle =
      sns_malloc_island_or_main(sizeof(sns_com_port_priv_handle));
  SNS_ASSERT(NULL != port_priv_handle); // We're in big trouble if we can't
                                        // allocate the SCP state

  if(!sns_island_is_island_ptr((intptr_t)port_handle))
  {
    // We ran out of island heap, so let's throw out a warning message and then
    // allocate the sync_com_port state in DDR. This isn't ideal for power, but
    // will help with stability.
    SNS_PRINTF(HIGH, sns_fw_printf, "Alloc SCP state in DDR!");
  }

  sns_memscpy(&port_priv_handle->com_config, sizeof(sns_com_port_config),
              com_config, sizeof(sns_com_port_config));

  port_priv_handle->bus_info.bus_type =
      (com_config->sub_sys_type != SNS_SUB_SYS_0) ? SNS_BUS_REMOTE
                                                  : com_config->bus_type;

  port_priv_handle->bus_info.power_on = false;
  port_priv_handle->bus_info.opened = false;
  /** bus_info::bus_config shall be allocated by bus specific
   *  implementation.
   */
  COM_PORT_PRINTF(HIGH, sns_fw_printf,
                  "Port handle %p, bus type: %d: with power status %d",
                  port_priv_handle, port_priv_handle->bus_info.bus_type,
                  port_priv_handle->bus_info.power_on);

  *port_handle = (sns_sync_com_port_handle *)port_priv_handle;

  port_priv_handle->caller = (void *)__builtin_return_address(0);
  if(!save_com_port(port_priv_handle))
  {
    sns_free(*port_handle);
    *port_handle = NULL;
    return SNS_RC_FAILED;
  }
  return SNS_RC_SUCCESS;
}
/*----------------------------------------------------------------------------*/

SNS_SECTION(".text.sns")
sns_rc sns_scp_get_dynamic_addr( sns_com_port_config const *com_config,
                                 uint32_t mipi_manufacturer_id,
                                 sns_com_port_i3c_slaves *i3c_slave_list,
                                 uint32_t *num_slaves )
{
  sns_rc return_code = SNS_RC_INVALID_VALUE;

  if(com_config != NULL || i3c_slave_list != NULL)
  {
    return_code = SNS_RC_FAILED;

    if(com_config != NULL && scp_port_apis[com_config->bus_type]->sns_scp_get_dynamic_addr != NULL)
    {
      return_code =
          scp_port_apis[com_config->bus_type]->sns_scp_get_dynamic_addr(
              com_config, mipi_manufacturer_id, i3c_slave_list, num_slaves);
    }
  }
  return return_code;
}

/*----------------------------------------------------------------------------*/
SNS_SECTION(".text.sns")
sns_rc sns_scp_issue_ccc(sns_sync_com_port_handle *port_handle,
                         sns_sync_com_port_ccc ccc_cmd, uint8_t *buffer,
                         uint32_t bytes, uint32_t *xfer_bytes)
{
  sns_com_port_priv_handle *priv_handle =
      (sns_com_port_priv_handle *)port_handle;
  sns_bus_info *bus_info;
  sns_rc return_code = SNS_RC_INVALID_VALUE;

  if(port_handle != NULL && SNS_SYNC_COM_PORT_CCC_ENEC <= ccc_cmd &&
     SNS_SYNC_COM_PORT_CCC_SETNEWDA >= ccc_cmd)
  {
    bus_info = &priv_handle->bus_info;
    return_code = SNS_RC_FAILED;

    if(scp_port_apis[bus_info->bus_type]->sns_scp_issue_ccc != NULL &&
       bus_info->opened == true && bus_info->power_on == true)
    {
      COM_PORT_PRINTF(HIGH, sns_fw_printf, "sns_scp_issue_ccc: ccc_cmd-%d",
                      ccc_cmd);
      return_code = scp_port_apis[bus_info->bus_type]->sns_scp_issue_ccc(
          port_handle, ccc_cmd, buffer, bytes, xfer_bytes);
    }
  }
  return return_code;
}
/*----------------------------------------------------------------------------*/

sns_sync_com_port_service_api scp_port_service_api SNS_SECTION(".data.sns") = {
    .struct_len = sizeof(sns_sync_com_port_service_api),
    .sns_scp_register_com_port = &sns_scp_register_com_port,
    .sns_scp_deregister_com_port = &sns_scp_deregister_com_port,
    .sns_scp_open = &sns_scp_open,
    .sns_scp_close = &sns_scp_close,
    .sns_scp_update_bus_power = &sns_scp_update_bus_power,
    .sns_scp_register_rw = &sns_scp_register_rw,
    .sns_scp_register_rw_ex = &sns_scp_register_rw_ex,
    .sns_scp_simple_rw = &sns_scp_simple_rw,
    .sns_scp_get_write_time = &sns_scp_get_write_time,
    .sns_scp_get_version = &sns_scp_get_version,
    .sns_scp_issue_ccc = &sns_scp_issue_ccc,
    .sns_scp_get_dynamic_addr = &sns_scp_get_dynamic_addr,
    .sns_scp_rw = &sns_scp_rw,
};

/*----------------------------------------------------------------------------*/

sns_sync_com_port_service *sns_sync_com_port_service_init(void)
{

  sync_com_port_service.api = &scp_port_service_api;
  sync_com_port_service.service.type = SNS_SYNC_COM_PORT_SERVICE;
  sync_com_port_service.service.version = scp_version.major;
  {
    sns_rc rc;
    sns_osa_lock_attr lock_attr;
    rc = sns_osa_lock_attr_init(&lock_attr);
    rc |= sns_osa_lock_init(&lock_attr, &com_port_list_lock);
    rc |= sns_osa_lock_init(&lock_attr, &proxy_com_port_lock);
    if(SNS_RC_SUCCESS != rc)
    {
      return NULL;
    }
    sns_memzero(&sns_proxy_com_ports[0], sizeof(sns_proxy_com_ports));
  }
  return &sync_com_port_service;
}
/*----------------------------------------------------------------------------*/

SNS_SECTION(".text.sns")
static bool save_proxy_com_port(sns_com_port_priv_handle *port_handle)
{
  bool found = false;

  SNS_PRINTF(HIGH, sns_fw_printf, "save_proxy_com_port: Port handle %p",
             port_handle);

  sns_osa_lock_acquire(&proxy_com_port_lock);
  for(uint8_t i = 0; i < ARR_SIZE(sns_proxy_com_ports); i++)
  {
    if(sns_proxy_com_ports[i].com_config.bus_instance ==
       port_handle->com_config.bus_instance)
    {
      found = true;
      sns_proxy_com_ports[i].ref_count++;
      break;
    }
  }
  if(!found)
  {
    for(uint8_t i = 0; i < ARR_SIZE(sns_proxy_com_ports); i++)
    {
      if(sns_proxy_com_ports[i].ref_count == 0)
      {
        found = true;
        sns_memscpy(&(sns_proxy_com_ports[i].com_config),
                    sizeof(sns_com_port_config), &(port_handle->com_config),
                    sizeof(sns_com_port_config));
        sns_proxy_com_ports[i].ref_count++;
        break;
      }
    }
    if(!found)
    {
      SNS_PRINTF(ERROR, sns_fw_printf, "save: 0x%X not saved", port_handle);
    }
  }
  sns_osa_lock_release(&proxy_com_port_lock);
  return found;
}
/*----------------------------------------------------------------------------*/

SNS_SECTION(".text.sns")
static bool remove_proxy_com_port(sns_com_port_priv_handle *port_handle)
{
  bool found = false;

  SNS_PRINTF(HIGH, sns_fw_printf, "remove_proxy_com_port: Port handle %p",
             port_handle);

  sns_osa_lock_acquire(&proxy_com_port_lock);
  for(uint8_t i = 0; i < ARR_SIZE(sns_proxy_com_ports); i++)
  {
    if(sns_proxy_com_ports[i].com_config.bus_instance ==
       port_handle->com_config.bus_instance)
    {
      found = true;
      SNS_ASSERT(sns_proxy_com_ports[i].ref_count > 0);
      sns_proxy_com_ports[i].ref_count--;
      if(sns_proxy_com_ports[i].ref_count == 0)
      {
        if(sns_proxy_com_ports[i].handle)
        {
          sns_scp_close(sns_proxy_com_ports[i].handle);
          sns_free(sns_proxy_com_ports[i].handle);
          sns_proxy_com_ports[i].handle = NULL;
        }
        sns_memzero(&sns_proxy_com_ports[i], sizeof(sns_com_port_proxy_handle));
      }
      break;
    }
  }
  if(!found)
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "remove: 0x%X not found", port_handle);
  }
  sns_osa_lock_release(&proxy_com_port_lock);
  return found;
}
/*----------------------------------------------------------------------------*/

SNS_SECTION(".text.sns")
void sns_scp_notify_q6_sensor_wr_update(int32_t wakeup_rate)
{
  bool high_perf_mode_on = false;
  if(wakeup_rate >= SNS_HIGH_PERF_MODE_WAKEUP_RATE_THRESHOLD)
  {
    high_perf_mode_on = true;
  }

  sns_osa_lock_acquire(&proxy_com_port_lock);
  for(uint8_t i = 0; i < ARR_SIZE(sns_proxy_com_ports); i++)
  {
    if(sns_proxy_com_ports[i].ref_count)
    {
      if(high_perf_mode_on != sns_proxy_com_ports[i].high_perf_mode_on)
      {
        SNS_PRINTF(HIGH, sns_fw_printf, "high_perf_mode_on: old(%d),new(%d)",
                   sns_proxy_com_ports[i].high_perf_mode_on, high_perf_mode_on);

        sns_proxy_com_ports[i].high_perf_mode_on = high_perf_mode_on;

        if(high_perf_mode_on)
        {
          if(sns_proxy_com_ports[i].handle == NULL)
          {
            sns_proxy_com_ports[i].handle =
                sns_malloc_island_or_main(sizeof(sns_com_port_priv_handle));

            sns_com_port_priv_handle *priv_handle =
                (sns_com_port_priv_handle *)(sns_proxy_com_ports[i].handle);

            sns_memscpy(&(priv_handle->com_config), sizeof(sns_com_port_config),
                        &(sns_proxy_com_ports[i].com_config),
                        sizeof(sns_com_port_config));

            if(priv_handle->com_config.sub_sys_type != SNS_SUB_SYS_0)
            {
              priv_handle->bus_info.bus_type = SNS_BUS_REMOTE;
            }
            else
            {
              priv_handle->bus_info.bus_type = priv_handle->com_config.bus_type;
            }

            sns_scp_open(sns_proxy_com_ports[i].handle);
          }
        }
        else
        {
          if(sns_proxy_com_ports[i].handle != NULL)
          {
            sns_scp_close(sns_proxy_com_ports[i].handle);
            sns_free(sns_proxy_com_ports[i].handle);
            sns_proxy_com_ports[i].handle = NULL;
          }
        }
      }
    }
  }
  sns_osa_lock_release(&proxy_com_port_lock);
}
/*----------------------------------------------------------------------------*/

/**
 * @file sns_interrupt_configure.c
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <stdint.h>

#include "sns_assert.h"
#include "sns_com_port_clk_resources.h"
#include "sns_com_port_priv.h"
#include "sns_interrupt.h"
#include "sns_island.h"
#include "sns_island_config.h"
#include "sns_memmgr.h"
#include "sns_mem_util.h"
#include "sns_osa_thread.h"
#include "sns_request.h"
#include "sns_sensor_instance.h"
#include "sns_service_manager.h"
#include "sns_time.h"
#include "sns_types.h"

#include "pb_decode.h"
#include "pb_encode.h"
#include "sns_pb_util.h"

/* Core includes: */
#include "i2c_api.h"
#include "island_user.h"
#include "uGPIOInt.h"

typedef struct
{
  sns_com_port_priv_handle com_port_handle;
  void *i2c_handle; /** short cut to the i2c handle inside com_port_handle */
} sns_interrupt_pal_config;

static const uGPIOIntTriggerType trigger_map[] = {
    UGPIOINT_TRIGGER_RISING,    UGPIOINT_TRIGGER_FALLING,
    UGPIOINT_TRIGGER_DUAL_EDGE, UGPIOINT_TRIGGER_HIGH,
    UGPIOINT_TRIGGER_LOW,
};

/**
 * Conversion function to convert bus IBI timestamps to QTimer timestamps
 *
 * @param[in] cb_type  Bus callback type
 *
 * @param[in] qup_ts   Timestamp provided by the QUP / bus driver
 *
 * @return QTimer timestamp of the IBI event
 */
SNS_SECTION(".text.sns")
static sns_time convert_qup_v3_ts(bus_callback_type cb_type, uint64_t qup_ts)
{
  sns_time ts = 0;
  sns_time now = sns_get_system_time();

  switch(cb_type)
  {
  case IBI_CALLBACK:
    // 32-bit shifted timestamp
    ts = (now & (UINT64_MAX << 32 << 7)) | (qup_ts << 7);

    if(ts > now)
    {
      // rollover -- subtract 1<<32<<7;
      ts -= 1ULL << 32 << 7;
    }
    break;
  case IBI_CALLBACK_TIMESTAMP_ASYNC0:
    ts = now;
    break;
  case IBI_CALLBACK_TIMESTAMP_48_BIT:
    ts = (now & (UINT64_MAX << 48)) | (qup_ts);

    if(ts > now)
    {
      // rollover -- subtract 1<<48;
      ts -= 1ULL << 48;
    }
    break;
  default:
    ts = now;
    SNS_PRINTF(ERROR, sns_fw_printf, "Unexpected IBI CB type %d", cb_type);
    break;
  }
  return ts;
}

SNS_SECTION(".text.sns")
static void sns_ibi_sensor_notify(bus_callback_type cb_type,
                                  callback_ctxt *cb_ctxt, void *usr_ctxt)
{
  sns_ibi_info *ibi_info = (sns_ibi_info *)usr_ctxt;
  ibi_cb_status *status = &cb_ctxt->ibi_status;
  sns_interrupt_ibi_data cb_data = {0};

  if(I2C_ERROR(status->status_ibi))
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "IBI error %d slave:%d",
               status->status_ibi, status->slave_address);
  }
  cb_data.timestamp = convert_qup_v3_ts(cb_type, status->ibi_timestamp);
  cb_data.num_ibi_bytes = status->ibi_rlen;
  if(NULL != ibi_info->notify)
  {
    ibi_info->notify(ibi_info->inst, &cb_data);
  }
  else
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "IBI callback not found!");
  }
}

/*
 Some interrupts are registered with this "exit_island" wrapper. This helps to
 avoid checking if the instance pointer is in island on every interrupt.
*/
SNS_SECTION(".text.sns")
static void sns_ibi_sensor_notify_exit_island(bus_callback_type cb_type,
                                              callback_ctxt *cb_ctxt,
                                              void *usr_ctxt)
{
  SNS_ISLAND_EXIT();
  sns_ibi_sensor_notify(cb_type, cb_ctxt, usr_ctxt);
}
/**
 * Register interrupt signal with uGPIOInt
 */
sns_rc sns_interrupt_gpio_register(sns_interrupt_gpio_register_param *reg_param)
{
  int32 err;
  sns_rc rc = SNS_RC_SUCCESS;
  uint32_t flags = 0;

  flags |= UGPIOINTF_ISLAND;

  if(reg_param->irq_info->trigger_type == SNS_INTERRUPT_TRIGGER_TYPE_HIGH ||
     reg_param->irq_info->trigger_type == SNS_INTERRUPT_TRIGGER_TYPE_LOW)
  {
    flags |= UGPIOINTF_DSR;
  }

  if(reg_param->irq_info->uses_hw_timestamp_unit)
  {
    flags |= UGPIOINTF_TIMESTAMP_EN;
  }

  err = uGPIOInt_RegisterInterrupt(
      reg_param->irq_info->irq_num,
      trigger_map[reg_param->irq_info->trigger_type],
      (uGPIOINTISR)reg_param->func, (uGPIOINTISRCtx)reg_param->inst, flags);
  if(err != UGPIOINT_SUCCESS)
  {
    rc = SNS_RC_FAILED;
  }
  else
  {
#ifndef SNS_DISABLE_ISLAND
#ifdef SNS_TARGET_DUAL_EDGE_INT_NO_ISLAND_SUPPORT
    if(reg_param->irq_info->trigger_type !=
       SNS_INTERRUPT_TRIGGER_TYPE_DUAL_EDGE)
#endif
    {
      uint32 l2vic_int_num = 0;

      /* Enable Island Interrupt */
      uGPIOInt_GetInterruptID(reg_param->irq_info->irq_num, &l2vic_int_num);
      err = island_mgr_add_interrupt_island(SNS_USLEEP_ISLAND, l2vic_int_num);
      SNS_VERBOSE_ASSERT(ISLAND_MGR_EOK == err,
                         "Failed to add l2vic_int_num: %d to island spec",
                         l2vic_int_num);
    }
#endif /* SNS_DISABLE_ISLAND */
  }

  return rc;
}

/**
 * Deregister interrupt signal with uGPIOInt
 */
sns_rc
sns_interrupt_gpio_deregister(sns_interrupt_gpio_register_param *reg_param)
{
  int32_t err;
  sns_rc rc = SNS_RC_SUCCESS;

#ifndef SNS_DISABLE_ISLAND

#ifdef SNS_TARGET_DUAL_EDGE_INT_NO_ISLAND_SUPPORT
  if(reg_param->irq_info->trigger_type != SNS_INTERRUPT_TRIGGER_TYPE_DUAL_EDGE)
#endif
  {
    uint32 l2vic_int_num = 0;

    /* Disable Island Interrupt */
    uGPIOInt_GetInterruptID(reg_param->irq_info->irq_num, &l2vic_int_num);
    err = island_mgr_remove_interrupt_island(SNS_USLEEP_ISLAND, l2vic_int_num);
    SNS_VERBOSE_ASSERT(ISLAND_MGR_EOK == err,
                       "Failed to remove l2vic_int_num: %d from island spec",
                       l2vic_int_num);
  }
#endif /* SNS_DISABLE_ISLAND */

  err = uGPIOInt_DeregisterInterrupt(reg_param->irq_info->irq_num);
  if(err != UGPIOINT_SUCCESS)
  {
    rc = SNS_RC_FAILED;
  }
  return rc;
}

SNS_SECTION(".text.sns")
sns_rc sns_interrupt_reenable(sns_sensor_instance *const this, uint32_t irq_num)
{
  int32_t ret = 0;
  sns_rc rv = SNS_RC_SUCCESS;
  ret = uGPIOInt_InterruptDone(irq_num);
  if(ret != UGPIOINT_SUCCESS)
  {
    SNS_INST_PRINTF(ERROR, this, "InterruptDone failed %d", ret);
    rv = SNS_RC_FAILED;
  }
  return rv;
}

/**
 * Attempt to read the timestamp that was captured by the interrupt's HW
 * timestamping unit.
 *
 * @param[i] this        pointer to the sensor instance, for logging purposes
 * @param[i] gpio        interrupt pin to read the timestamp from
 * @param[o] timestamp   where the timestamp is placed
 *
 * @return sns_rc        SNS_RC_SUCCESS if the timestamp was successfully read,
 *                       SNS_RC_FAILED otherwise
 */
SNS_SECTION(".text.sns")
sns_rc sns_interrupt_read_timestamp(sns_sensor_instance *const this,
                                    uint32_t gpio, sns_time *timestamp,
                                    sns_interrupt_timestamp_status *ts_status)
{
  int32_t ret;
  sns_rc rv = SNS_RC_SUCCESS;
  uGPIOIntTimestampStatus gpio_ts_status = UGPIOINT_TSS_INVALID;
  UNUSED_VAR(this);

  ret = uGPIOInt_TimestampRead(gpio, &gpio_ts_status, timestamp);
  // If uGPIOInt_TimestampRead returns an error code, that means that we hit a
  // rare timing error related to PDC and island mode. Try running
  // uGPIOInt_TimestampRead again which should allow us to gracefully handle
  // the timing problem. The second time we call it, it should go through.
  if(ret != UGPIOINT_SUCCESS)
  {
    ret = uGPIOInt_TimestampRead(gpio, &gpio_ts_status, timestamp);
  }

  // UGPIOINT_TSS_VALID and UGPIOINT_TSS_VALID_OVERFLOW are both acceptable
  // statuses
  if((ret != UGPIOINT_SUCCESS) ||
     ((gpio_ts_status != UGPIOINT_TSS_VALID) &&
      (gpio_ts_status != UGPIOINT_TSS_VALID_OVERFLOW)))
  {
    // SNS_INST_PRINTF(ERROR, this, "read_timestamp failed %d, ts_status %u",
    // ret, gpio_ts_status);
    rv = SNS_RC_FAILED;
  }

  *ts_status = (sns_interrupt_timestamp_status)gpio_ts_status;
  return rv;
}

SNS_SECTION(".text.sns")
static sns_qup_type sns_intr_map_qup_type(sns_intr_qup_type qup_type)
{
  switch(qup_type)
  {
  case SNS_INTR_SSC_QUP:
    return SNS_SSC_QUP;
  case SNS_INTR_TOP_QUP:
    return SNS_TOP_QUP;
  default:
    return SNS_SSC_QUP;
  }
}

/**
 * Register IBI with bus driver

 * @param[i] this    current instance
 * @param[i] req     IRQ config request
 *
 * @return
 * SNS_RC_INVALID_VALUE - invalid request
 * SNS_RC_SUCCESS
 */
SNS_SECTION(".text.sns")
sns_rc sns_interrupt_register_ibi(sns_sensor_instance *const this,
                                  sns_ibi_req const *new_req,
                                  sns_ibi_req *ibi_req, sns_ibi_info *ibi_info)
{
  sns_service_manager *service_manager = this->cb->get_service_manager(this);
  sns_sync_com_port_service *scp_service =
      (sns_sync_com_port_service *)service_manager->get_service(
          service_manager, SNS_SYNC_COM_PORT_SERVICE);
  i2c_status bus_status;
  sns_i2c_info *i2c_info;
  sns_bus_info *bus_info;
  sns_interrupt_pal_config *pal_config;
  sns_com_port_config *com_config;
  callback_func sns_ibi_notify_fptr;
  sns_rc rc;

  if(NULL == ibi_info || NULL == ibi_info->notify)
  {
    return SNS_RC_INVALID_VALUE;
  }

  if(NULL != ibi_info->pal_config)
  {
    /* If the IBI is registered then proceed only if the incoming request is
       diffrent from current config.*/
    if(0 == sns_memcmp(new_req, ibi_req, sizeof(*new_req)))
    {
      /* new_req is same as current config. No-op.*/
      return SNS_RC_SUCCESS;
    }
    /* Request changed, so deregister the old IBI */
    (void)sns_interrupt_deregister_ibi(this, ibi_req, ibi_info);
  }

  if(sns_island_is_island_ptr((intptr_t)ibi_info))
  {
    sns_ibi_notify_fptr = &sns_ibi_sensor_notify;
  }
  else
  {
    sns_ibi_notify_fptr = &sns_ibi_sensor_notify_exit_island;
  }

  pal_config = (sns_interrupt_pal_config *)sns_malloc_island_or_main(
      sizeof(sns_interrupt_pal_config));
  SNS_ASSERT(NULL != pal_config);

  bus_info = &pal_config->com_port_handle.bus_info;
  com_config = &pal_config->com_port_handle.com_config;

  com_config->qup_type = sns_intr_map_qup_type(
      new_req->has_qup_type ? new_req->qup_type : SNS_INTR_SSC_QUP);
  com_config->qup_instance =
      new_req->has_qup_instance ? new_req->qup_instance : 0;
  com_config->bus_type = SNS_BUS_I3C;
  com_config->bus_instance = new_req->bus_instance;
  com_config->max_bus_speed_KHz = I3C_SDR_DATA_RATE_12500_KHZ;
  com_config->slave_control = new_req->dynamic_slave_addr;

  rc = scp_service->api->sns_scp_open(
      (sns_sync_com_port_handle *)&pal_config->com_port_handle);

  i2c_info = (sns_i2c_info *)bus_info->bus_config;
  pal_config->i2c_handle = i2c_info->i2c_handle;

  sns_memscpy(ibi_req, sizeof(*ibi_req), new_req, sizeof(*new_req));
  ibi_info->timestamp = 0;
  ibi_info->ibi_data_buf = sns_malloc(SNS_HEAP_PRAM, new_req->ibi_data_bytes);
  if(NULL == ibi_info->ibi_data_buf)
  {
    ibi_req->ibi_data_bytes = 0;
  }

  i2c_power_on(pal_config->i2c_handle);

  bus_status =
      i3c_register_ibi(pal_config->i2c_handle, ibi_req->dynamic_slave_addr,
                       sns_ibi_notify_fptr, ibi_info, ibi_info->ibi_data_buf,
                       ibi_req->ibi_data_bytes, IBI_TIMESTAMP_ASYNC0);

  if(I2C_SUCCESS(bus_status))
  {
    ibi_info->pal_config = pal_config;
#if !defined(SNS_I2C_BUS_PWR_OFF)
    i2c_power_off(pal_config->i2c_handle);
#endif
  }
  else
  {
    i2c_status bus_status;
    bus_status = i2c_power_off(pal_config->i2c_handle);
    rc = scp_service->api->sns_scp_close(
        (sns_sync_com_port_handle *)&pal_config->com_port_handle);
    SNS_INST_PRINTF(ERROR, this,
                    "Failed to register ibi error: slave_addr=0x%x instance=%d "
                    "ibi_data_buf=%p cleanup status (%d, %d)",
                    ibi_req->dynamic_slave_addr, ibi_req->bus_instance,
                    ibi_info->ibi_data_buf, bus_status, rc);
    sns_memzero(ibi_req, sizeof(*ibi_req));
    sns_free(pal_config);
    return SNS_RC_INVALID_VALUE;
  }

  return SNS_RC_SUCCESS;
}

/**
 * De-Register the IBI

 * @param[i] this    current instance
 *
 * @return
 * SNS_RC_INVALID_VALUE - invalid request
 * SNS_RC_SUCCESS
 */
SNS_SECTION(".text.sns")
sns_rc sns_interrupt_deregister_ibi(sns_sensor_instance *const this,
                                    sns_ibi_req *ibi_req,
                                    sns_ibi_info *ibi_info)
{
  sns_rc rc;

  if(NULL != ibi_info->pal_config)
  {
    i2c_status err = I2C_SUCCESS;
    i2c_status bus_status = I2C_SUCCESS;
    sns_interrupt_pal_config *pal_config =
        (sns_interrupt_pal_config *)ibi_info->pal_config;
    sns_service_manager *service_manager = this->cb->get_service_manager(this);
    sns_sync_com_port_service *scp_service =
        (sns_sync_com_port_service *)service_manager->get_service(
            service_manager, SNS_SYNC_COM_PORT_SERVICE);

    // IBI de-registration needs power on.
#if !defined(SNS_I2C_BUS_PWR_OFF)
    i2c_power_on(pal_config->i2c_handle);
#endif

#if !(defined(SSC_TARGET_NO_I3C_SUPPORT))
    err =
        i3c_deregister_ibi(pal_config->i2c_handle, ibi_req->dynamic_slave_addr);
#endif

#if !defined(SNS_I2C_BUS_PWR_OFF)
    bus_status = i2c_power_off(pal_config->i2c_handle);
#endif
    rc = scp_service->api->sns_scp_close(
        (sns_sync_com_port_handle *)&pal_config->com_port_handle);

    sns_free(ibi_info->pal_config);
    sns_free(ibi_info->ibi_data_buf);
    ibi_info->pal_config = NULL;
    ibi_info->ibi_data_buf = NULL;
    sns_memzero(ibi_req, sizeof(*ibi_req));
    if(I2C_ERROR(err))
    {
      SNS_INST_PRINTF(ERROR, this, "Failed to de-register ibi err: %d", err);
      return SNS_RC_INVALID_VALUE;
    }
    if(I2C_ERROR(bus_status) || (rc != SNS_RC_SUCCESS))
    {
      SNS_INST_PRINTF(ERROR, this,
                      "Error cleaning up after IBI dereg: bus_status %d, rc %d",
                      bus_status, rc);
    }
  }
  return SNS_RC_SUCCESS;
}

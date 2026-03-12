/** ============================================================================
 *  @file
 *
 *  @brief This file contains API for gpio utility
 *
 *  @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its
 * subsidiaries. All rights reserved. Confidential and Proprietary -
 * Qualcomm Technologies, Inc.
 *
 *  ==========================================================================*/

/*==============================================================================
  Include Files
  ============================================================================*/

#include <stdint.h>
#include <string.h>

#include "sns_fw_gpio_service_internal.h"
#include "sns_gpio.h"
#include "sns_interrupt.pb.h"
#include "sns_island.h"
#include "sns_memmgr.h"
#include "sns_osa_lock.h"
#include "sns_printf_int.h"
#include "sns_types.h"
#include "sns_ulog.h"

#include "DalDevice.h"
#include "GPIO.h"
#include "GPIOTypes.h"

/*=============================================================================
  Macros
  ===========================================================================*/

#define SNS_GPIO_DRIVE_STRENGTH_RESOLUTION 200

/*=============================================================================
  Type Definitions
  ===========================================================================*/

typedef struct sns_gpio_state_priv
{
  /*device handle for tlmm GPIOs*/
  GPIOClientHandleType gpio_device_handle_tlmm;
  /*device handle for SSC GPIOs*/
  GPIOClientHandleType gpio_device_handle_ssc_lpi;

  sns_osa_lock lock;
} sns_gpio_state_priv;

/*=============================================================================
  Static Data Definitions
  ===========================================================================*/

static sns_gpio_state_priv gpio_state SNS_SECTION(".data.sns");

static const GPIOPullType
    gpio_pull_map[_sns_interrupt_pull_type_ARRAYSIZE] SNS_SECTION(
        ".rodata.sns") = {
        GPIO_NP,
        GPIO_PD,
        GPIO_KP,
        GPIO_PU,
};

/*=============================================================================
  Static Function Definitions
  ===========================================================================*/

/**
 * Return pull type value from GPIOPullType enum.
 *
 * @param[i] pull   sns_interrupt_pull_type type
 *
 * @return GPIOPullType
 */
SNS_SECTION(".text.sns")
static GPIOPullType get_gpio_pull_type(sns_interrupt_pull_type pull)
{
  return gpio_pull_map[pull];
}

/**
 * Return drive strength
 *
 * @param[i] drive   sns_interrupt_drive_strength type
 *
 * @return GPIO Drive value
 */
SNS_SECTION(".text.sns")
static uint32_t get_gpio_drive_type(sns_interrupt_drive_strength drive)
{
  return ((drive + 1) * SNS_GPIO_DRIVE_STRENGTH_RESOLUTION);
}

/*=============================================================================
  Public Function Definitions
  ===========================================================================*/

sns_rc sns_gpio_init(void)
{
  sns_osa_lock_attr lock_attr;
  sns_rc rc;

  rc = sns_osa_lock_attr_init(&lock_attr);
  rc |= sns_osa_lock_init(&lock_attr, &gpio_state.lock);
  if(SNS_RC_SUCCESS == rc)
  {
    if(GPIO_SUCCESS !=
       GPIO_Attach(GPIO_DEVICE_TLMM, &gpio_state.gpio_device_handle_tlmm))
    {
      SNS_ULOG_INIT_SEQ_ERR("%s: TLMM GPIO attach failed", __FUNCTION__);
      rc = SNS_RC_FAILED;
    }
    if(GPIO_SUCCESS !=
       GPIO_Attach(GPIO_DEVICE_SSC_LPI, &gpio_state.gpio_device_handle_ssc_lpi))
    {
      SNS_ULOG_INIT_SEQ_ERR("%s: SSC LPI GPIO attach failed", __FUNCTION__);
      rc = SNS_RC_FAILED;
    }
  }
  if(SNS_RC_SUCCESS != rc)
  {
    sns_osa_lock_deinit(&gpio_state.lock);
  }

  return rc;
}

SNS_SECTION(".text.sns")
sns_rc sns_gpio_read(sns_gpio_read_config *config, sns_gpio_state *state)
{
  uint32_t gpio = config->gpio;
  bool is_chip_pin = config->is_chip_pin;
  GPIOValueType gpio_val = GPIO_LOW;
  GPIOKeyType GPIO_Key;
  GPIOClientHandleType gpio_handle = gpio_state.gpio_device_handle_ssc_lpi;

  if(is_chip_pin)
  {
    /**TLMM API are not available in island mode. Exit
     *  island mode. */
    SNS_ISLAND_EXIT();

    gpio_handle = gpio_state.gpio_device_handle_tlmm;
  }

  sns_osa_lock_acquire(&gpio_state.lock);
  if(GPIO_SUCCESS !=
     GPIO_RegisterPinExplicit(gpio_handle, gpio, GPIO_ACCESS_SHARED, &GPIO_Key))
  {
    sns_osa_lock_release(&gpio_state.lock);
    SNS_PRINTF(ERROR, sns_fw_printf, "GPIO_RegisterPinExplicit gpio %d", gpio);
    return SNS_RC_NOT_SUPPORTED;
  }

  if(GPIO_SUCCESS != GPIO_ReadPin(gpio_handle, GPIO_Key, &gpio_val))
  {
    sns_osa_lock_release(&gpio_state.lock);
    SNS_PRINTF(ERROR, sns_fw_printf, "GPIO_ReadPin gpio %d", gpio);
    return SNS_RC_NOT_SUPPORTED;
  }

  *state = (gpio_val == GPIO_LOW) ? SNS_GPIO_STATE_LOW : SNS_GPIO_STATE_HIGH;
  sns_osa_lock_release(&gpio_state.lock);

  return SNS_RC_SUCCESS;
}

SNS_SECTION(".text.sns")
sns_rc sns_gpio_write(sns_gpio_write_config *config)
{
  uint32_t gpio = config->gpio;
  bool is_chip_pin = config->is_chip_pin;
  sns_gpio_drive_strength drive_strength = config->drive_strength;
  sns_gpio_pull_type pull = config->pull;
  sns_gpio_state state = config->state;

  GPIOKeyType GPIO_Key;
  GPIOConfigType gpio_cfg;
  GPIOResult return_code;
  GPIOValueType out_val = (state == SNS_GPIO_STATE_LOW) ? GPIO_LOW : GPIO_HIGH;
  GPIOClientHandleType gpio_handle = gpio_state.gpio_device_handle_ssc_lpi;

  if(is_chip_pin)
  {
    /** TLMM APIs are not available in island mode. Exit island
     *  mode. */
    SNS_ISLAND_EXIT();
    gpio_handle = gpio_state.gpio_device_handle_tlmm;
  }

  sns_osa_lock_acquire(&gpio_state.lock);
  if(GPIO_SUCCESS !=
     GPIO_RegisterPinExplicit(gpio_handle, gpio, GPIO_ACCESS_SHARED, &GPIO_Key))
  {
    sns_osa_lock_release(&gpio_state.lock);
    return SNS_RC_NOT_SUPPORTED;
  }

  gpio_cfg.func = 0;
  gpio_cfg.dir = GPIO_OUT;
  gpio_cfg.pull = get_gpio_pull_type((sns_interrupt_pull_type)pull);
  gpio_cfg.drive =
      get_gpio_drive_type((sns_interrupt_drive_strength)drive_strength);

  return_code = GPIO_ConfigPin(gpio_handle, GPIO_Key, gpio_cfg);
  if(return_code != GPIO_SUCCESS)
  {
    sns_osa_lock_release(&gpio_state.lock);
    SNS_PRINTF(ERROR, sns_fw_printf, "Configuration of GPIO: %d failed", gpio);
    return SNS_RC_FAILED;
  }

  /* SNS_PRINTF(
       HIGH, sns_fw_printf, "write_gpio gpio %d out_val = %d", gpio, out_val);
   */
  if(GPIO_SUCCESS != GPIO_WritePin(gpio_handle, GPIO_Key, out_val))
  {
    sns_osa_lock_release(&gpio_state.lock);
    SNS_PRINTF(ERROR, sns_fw_printf, "GPIO_WritePin gpio %d", gpio);
    return SNS_RC_NOT_SUPPORTED;
  }
  sns_osa_lock_release(&gpio_state.lock);

  return SNS_RC_SUCCESS;
}

SNS_SECTION(".text.sns")
sns_rc sns_gpio_configure_interrupt(sns_gpio_interrupt_config *config)
{
  sns_interrupt_req const *req = config->req;
  bool enable = config->enable;
  bool set_inactive_cfg = config->set_inactive_cfg;

  GPIOResult err;
  sns_rc rc = SNS_RC_FAILED;
  GPIOConfigType gpio_cfg;
  GPIOKeyType GPIO_Key;
  GPIOClientHandleType gpio_handle =
      (req->is_chip_pin == true) ? gpio_state.gpio_device_handle_tlmm
                                 : gpio_state.gpio_device_handle_ssc_lpi;

  gpio_cfg.func = 0;
  gpio_cfg.dir = GPIO_IN;
  gpio_cfg.pull = get_gpio_pull_type(req->interrupt_pull_type);
  gpio_cfg.drive = get_gpio_drive_type(req->interrupt_drive_strength);

  sns_osa_lock_acquire(&gpio_state.lock);
  if(GPIO_SUCCESS != GPIO_RegisterPinExplicit(gpio_handle, req->interrupt_num,
                                              GPIO_ACCESS_SHARED, &GPIO_Key))
  {
    sns_osa_lock_release(&gpio_state.lock);
    return SNS_RC_FAILED;
  }

  if(req->is_chip_pin)
  {
    SNS_ISLAND_EXIT();
    /** Configuring chip level TLMM wake up interrupts may not be
     *  needed. The default config of these pins is done in the
     *  boot image. This functionality is no-op in older DDF
     *  architecture as well. */

    /* Configure INACTIVE CFG for this GPIO */
    if(set_inactive_cfg)
    {
      if(GPIO_SUCCESS !=
         GPIO_SetInactiveConfig(gpio_state.gpio_device_handle_tlmm, GPIO_Key,
                                gpio_cfg, gpio_cfg.pull & 0x01))
      {
        SNS_PRINTF(ERROR, sns_fw_printf, "GPIO_SetInactiveConfig IRQ %d failed",
                   req->interrupt_num);
      }
    }
    if(enable)
    {
      err = GPIO_ConfigPin(gpio_handle, GPIO_Key, gpio_cfg);
    }
    else
    {
      err = GPIO_ConfigPinInactive(gpio_handle, GPIO_Key);
    }
    sns_osa_lock_release(&gpio_state.lock);

    rc = (err == GPIO_SUCCESS) ? SNS_RC_SUCCESS : SNS_RC_FAILED;
  }

  return rc;
}

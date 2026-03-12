/**
 * @file sns_gpio_service.c
 *
 * @brief The GPIO Service implementation
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 **/

#include "sns_fw_gpio_service_internal.h"
#include "sns_gpio.h"
#include "sns_island.h"
#include "sns_memmgr.h"
#include "sns_osa_lock.h"
#include "sns_printf_int.h"
#include "sns_types.h"
#include "sns_ulog.h"
#include <stdint.h>
#include <string.h>

/** Version: */
static const uint16_t sns_gpio_service_version SNS_SECTION(".rodata.sns") = 1;

struct sns_fw_gpio_service
{
  sns_gpio_service service;
};

/**
 * Private state of the GPIO Service.
 */
static sns_fw_gpio_service gpio_service SNS_SECTION(".data.sns");

/** See sns_gpio_service.h */
SNS_SECTION(".text.sns")
static sns_rc read_gpio(uint32_t gpio, bool is_chip_pin, sns_gpio_state *state)
{
  sns_gpio_read_config config;
  config.gpio = gpio;
  config.is_chip_pin = is_chip_pin;
  return sns_gpio_read(&config, state);
}

/** See sns_gpio_service.h */
SNS_SECTION(".text.sns")
static sns_rc write_gpio(uint32_t gpio, bool is_chip_pin,
                         sns_gpio_drive_strength drive_strength,
                         sns_gpio_pull_type pull, sns_gpio_state state)
{
  sns_gpio_write_config config;
  config.gpio = gpio;
  config.is_chip_pin = is_chip_pin;
  config.drive_strength = drive_strength;
  config.pull = pull;
  config.state = state;
  return sns_gpio_write(&config);
}

/**
 * GPIO Service API.
 */
static const sns_gpio_service_api gpio_service_api
    SNS_SECTION(".rodata.sns") = {.struct_len = sizeof(sns_gpio_service_api),
                                  .read_gpio = &read_gpio,
                                  .write_gpio = &write_gpio};

/**
 * See sns_gpio_service_internal.h
 */
sns_fw_gpio_service *sns_gpio_service_init(void)
{

  gpio_service.service.api = &gpio_service_api;
  gpio_service.service.service.type = SNS_GPIO_SERVICE;
  gpio_service.service.service.version = sns_gpio_service_version;

  return &gpio_service;
}

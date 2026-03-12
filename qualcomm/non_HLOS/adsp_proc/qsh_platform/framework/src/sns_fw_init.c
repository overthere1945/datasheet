/*=============================================================================
  @file sns_fw_init.c

  Single entry point to the Sensors Framework.  Initializes all other core
  Framework modules, and all known services.

  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  ===========================================================================*/

/*=============================================================================
  Include Files
  ===========================================================================*/

#include "sns_hw_info.h"
#include "sns_island.h"
#include "sns_printf_int.h"
#include "sns_fw_sensor.h"
#include "sns_fw_sensor_instance.h"
#include "sns_fw_service_manager.h"
#include "sns_ccd_service_internal.h"
#include "sns_island.h"
#include "sns_island_util.h"
#include "sns_printf_int.h"
#include "sns_pwr_sleep_mgr.h"
#include "sns_qshtech_island_internal.h"
#include "sns_signal.h"
#include "sns_thread_manager.h"
#include "sns_ulog.h"
#include "sns_watchdog.h"

/*=============================================================================
  External Data Declarations
  ===========================================================================*/
#ifdef SNS_EXTERNAL_SDC_AVAILABLE
extern sns_rc sns_sdc_control_common_utility_init(void);
#endif
#if !defined(SSC_TARGET_X86) && !defined(SSC_TARGET_WEARABLES_AON)
extern const sns_register_entry sns_static_drivers_list[];
extern const uint32_t sns_static_drivers_list_len;
#endif
#ifdef SSC_QSH_SENSORS_LIST
extern const sns_register_entry qsh_static_sensors_list[];
extern const uint32_t qsh_static_sensors_list_len;
#endif
#ifdef QSH_ALGORITHMS_LIST
extern const sns_register_entry qsh_static_algorithms_list[];
extern const uint32_t qsh_static_algorithms_list_len;
#endif
#ifdef SSC_ALGORITHMS_LIST
extern const sns_register_entry ssc_static_algorithms_list[];
extern const uint32_t ssc_static_algorithms_list_len;
#endif
extern const sns_register_entry sns_static_sensors_group_0_list[];
extern const uint32_t sns_static_sensors_group_0_list_len;
extern const sns_register_entry sns_static_sensors_group_1_list[];
extern const uint32_t sns_static_sensors_group_1_list_len;
extern sns_sensor_cb sensor_cb;

/*=============================================================================
  Static Variable Definitions
  ===========================================================================*/
static bool sns_platform_in_slt SNS_SECTION(".data.sns") = false;

/*=============================================================================
  Static Function Definitions
  ===========================================================================*/

/**
 * Load and initialize all Sensors statically-compiled with the SLPI image.
 * Dynamically loaded Sensors will be loaded later.
 */
static sns_rc register_sensors(const sns_register_entry *sensor_list,
                               uint32_t list_len)
{
  sns_rc rc = SNS_RC_SUCCESS;

  sns_register_cb reg_cb = (sns_register_cb){.struct_len = sizeof(reg_cb),
                                             .init_sensor = &sns_sensor_init};

  for(int i = 0; i < list_len && SNS_RC_SUCCESS == rc; i++)
  {
    SNS_PRINTF(LOW, sns_fw_printf, "Register Sensor from static list %i",
               sensor_list[i].cnt);
    for(int j = 0; j < sensor_list[i].cnt && SNS_RC_SUCCESS == rc; j++)
    {
      sns_isafe_list_iter iter;
      sns_sensor_library *library = sns_sensor_library_init(
          sensor_list[i].func, j, sensor_list[i].is_islandLib,
          sensor_list[i].is_multithreaded, sensor_list[i].mem_segment);
      if(NULL == library)
      {
        SNS_ULOG_INIT_SEQ_ERR("%s: failed to initialize library, reg_func=%X",
                              __FUNCTION__, sensor_list[i].func);
        rc = SNS_RC_FAILED;
        break;
      }

      sns_osa_lock_acquire(library->library_lock);
      rc = sensor_list[i].func(&reg_cb);
      atomic_store(&library->removing, SNS_LIBRARY_ACTIVE);

      sns_isafe_list_iter_init(&iter, &library->sensors, true);
      if(0 == sns_isafe_list_iter_len(&iter))
      {
        SNS_ULOG_INIT_SEQ_ERR("%s: empty library, reg_func=%X", __FUNCTION__,
                              sensor_list[i].func);
        sns_sensor_library_deinit(library);
      }
      sns_osa_lock_release(library->library_lock);
    }
  }
  if(SNS_RC_SUCCESS != rc)
  {
    SNS_ULOG_INIT_SEQ_ERR("%s: failed sensor list %X", __FUNCTION__,
                          sensor_list);
  }
  return rc;
}

static void get_platform_slt_key(void)
{
  uint32_t key = 0;
  sns_hw_platform_slt_key slt_key = {.key = &key};
  sns_rc rc = sns_get_hw_info(SNS_HW_INFO_TYPE_PLATFORM_SLT_KEY, &slt_key);
  if(SNS_RC_SUCCESS == rc)
  {
    sns_platform_in_slt = (key == 1);
  }
}

/*=============================================================================
  Public Function Declarations
  ===========================================================================*/

/**
 * Initialize the Sensors Framework; all state, threads, and sensors.
 *
 * @return -1 upon error
 */
int sns_fw_init(void)
{
  sns_rc rc;
  uint8_t steps_completed = 0;
  SNS_PRINTF(LOW, sns_fw_printf, "Init started");
  get_platform_slt_key();
  rc = qsh_island_init();
  if(SNS_RC_SUCCESS == rc)
  {
    steps_completed++;
    rc = sns_watchdog_init();
  }
  if(SNS_RC_SUCCESS == rc)
  {
    steps_completed++;
    rc = sns_island_init();
  }
  if(SNS_RC_SUCCESS == rc)
  {
    steps_completed++;
    rc = sns_qshtech_island_init();
  }
  if(SNS_RC_SUCCESS == rc)
  {
    steps_completed++;
    rc = sns_thread_manager_init();
  }
  if(SNS_RC_SUCCESS == rc)
  {
    steps_completed++;
    rc = sns_sensor_init_fw();
  }
  if(SNS_RC_SUCCESS == rc)
  {
    steps_completed++;
    rc = sns_pwr_sleep_mgr_init();
  }
  if(SNS_RC_SUCCESS == rc)
  {
    steps_completed++;
    rc = sns_service_manager_init();
  }
  if(SNS_RC_SUCCESS == rc)
  {
    steps_completed++;
    rc = sns_signal_util_init();
  }
#ifdef SNS_EXTERNAL_SDC_AVAILABLE
  if(SNS_RC_SUCCESS == rc)
  {
    steps_completed++;
    rc = sns_sdc_control_common_utility_init();
  }
#endif
  if(SNS_RC_SUCCESS == rc)
  {
    steps_completed++;
    rc = sns_ccd_service_init();
  }
  if(SNS_RC_SUCCESS == rc)
  {
    steps_completed++;
    rc = sns_sensor_instance_init_fw();
  }
  if(SNS_RC_SUCCESS == rc)
  {
    steps_completed++;
    rc = register_sensors(sns_static_sensors_group_0_list,
                          sns_static_sensors_group_0_list_len);
  }
#if !defined(SSC_TARGET_X86) && !defined(SSC_TARGET_WEARABLES_AON)
  if(SNS_RC_SUCCESS == rc)
  {
    steps_completed++;
    rc = register_sensors(sns_static_drivers_list, sns_static_drivers_list_len);
  }
#endif
  if(SNS_RC_SUCCESS == rc)
  {
    steps_completed++;
    rc = register_sensors(sns_static_sensors_group_1_list,
                          sns_static_sensors_group_1_list_len);
  }

#ifdef SSC_ALGORITHMS_LIST
  if(SNS_RC_SUCCESS == rc)
  {
    steps_completed++;
    rc = register_sensors(ssc_static_algorithms_list,
                          ssc_static_algorithms_list_len);
  }
#endif

#ifdef SSC_QSH_SENSORS_LIST
  if(SNS_RC_SUCCESS == rc)
  {
    steps_completed++;
    rc = register_sensors(qsh_static_sensors_list, qsh_static_sensors_list_len);
  }
#endif

#ifdef QSH_ALGORITHMS_LIST
  if(SNS_RC_SUCCESS == rc)
  {
    steps_completed++;
    rc = register_sensors(qsh_static_algorithms_list,
                          qsh_static_algorithms_list_len);
  }
#endif

  if(SNS_RC_SUCCESS == rc)
  {
    steps_completed++;
    // Re-enable the thread manager; disabled by default in
    // sns_thread_manager_init
    sns_thread_manager_disable(false);
  }

  SNS_ULOG_INIT_SEQ("%s: steps completed = %u, rc = %u", __FUNCTION__,
                    steps_completed, rc);
  UNUSED_VAR(steps_completed);

  return (rc == SNS_RC_SUCCESS) ? 0 : -1;
}

SNS_SECTION(".text.sns") bool sns_system_is_in_slt_mode(void)
{
  return sns_platform_in_slt;
}

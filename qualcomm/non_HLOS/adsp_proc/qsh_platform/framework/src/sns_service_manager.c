/**============================================================================
  @file sns_service_manager.c

  Provides access to all service objects.  Sensors must query for the
  needed service each time they need it; the Framework reserves the right
  to move a service into/out of island mode, and pointers kept to it may
  break.

  It would be appropriate within sns_sensor::init to query for all required
  services, to ensure the Sensor is capable of operating.

  @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  ===========================================================================*/

/*=============================================================================
  Include Files
  ===========================================================================*/
#include <stdlib.h>

#include "sns_fw_attribute_service.h"
#include "sns_fw_basic_services.h"
#include "sns_fw_diag_service.h"
#include "sns_fw_event_service.h"
#include "sns_fw_memory_service.h"
#include "sns_fw_service_manager.h"
#include "sns_fw_gpio_service_internal.h"
#include "sns_island.h"
#include "sns_service_manager.h"
#include "sns_fw_sfe_service.h"
#include "sns_ulog.h"

/*=============================================================================
  Static Data Definitions
  ===========================================================================*/

static sns_fw_service_manager service_manager SNS_SECTION(".data.sns");

/*=============================================================================
  Static Function Definitions
  ===========================================================================*/

SNS_SECTION(".text.sns")
static sns_service *get_service(struct sns_service_manager *svc_mngr,
                                sns_service_type service)
{
  sns_fw_service_manager *this = (sns_fw_service_manager *)svc_mngr;
  if(service >= SNS_SERVICE_TYPE_LENGTH)
    return NULL;

  if(!sns_island_is_island_ptr((intptr_t)this->services[service]))
  {
    SNS_ISLAND_EXIT();
  }

  return this->services[service];
}

static sns_service *empty_sfe_service(sns_service_manager *svc_mgr)
{
  UNUSED_VAR(svc_mgr);
  return NULL;
}

sns_service *sns_sfe_service_init(sns_service_manager *svc_mgr)
    __attribute__((weak, alias("empty_sfe_service")));

/*=============================================================================
  Public Function Definitions
  ===========================================================================*/

SNS_SECTION(".text.sns") sns_fw_service_manager *sns_service_manager_ref(void)
{
  return &service_manager;
}

sns_rc sns_service_manager_init(void)
{
  sns_rc rc = SNS_RC_SUCCESS;

  service_manager.service.struct_len = sizeof(service_manager.service);
  service_manager.service.get_service = &get_service;

  service_manager.services[SNS_STREAM_SERVICE] =
      (sns_service *)sns_stream_service_init();
  service_manager.services[SNS_ATTRIBUTE_SERVICE] =
      (sns_service *)sns_attr_svc_init();
  service_manager.services[SNS_DYNAMIC_BUFFER_SERVICE] =
      (sns_service *)0xBADDBADD; // unused
  service_manager.services[SNS_EVENT_SERVICE] =
      (sns_service *)sns_event_service_init();
  service_manager.services[SNS_DIAG_SERVICE] =
      (sns_service *)sns_diag_service_init();
  service_manager.services[SNS_POWER_RAIL_SERVICE] =
      (sns_service *)sns_pwr_rail_service_init();
  service_manager.services[SNS_SYNC_COM_PORT_SERVICE] =
      (sns_service *)sns_sync_com_port_service_init();
  service_manager.services[SNS_GPIO_SERVICE] =
      (sns_service *)sns_gpio_service_init();
  service_manager.services[SNS_ISLAND_SERVICE] =
      (sns_service *)sns_island_service_init();
  service_manager.services[SNS_REGISTRATION_SERVICE] =
      (sns_service *)sns_registration_service_init();
  service_manager.services[SNS_POWER_MGR_SERVICE] =
      (sns_service *)sns_power_mgr_service_init();
  service_manager.services[SNS_MEMORY_SERVICE] =
      (sns_service *)sns_memory_service_init();
  service_manager.services[SNS_FILE_SERVICE] =
      (sns_service *)sns_file_service_init();
  service_manager.services[SNS_SFE_SERVICE] =
      (sns_service *)sns_sfe_service_init(
          (sns_service_manager *)&service_manager);

  for(int i = 0; i < ARR_SIZE(service_manager.services); i++)
  {
    if(NULL == service_manager.services[i] &&
#ifdef SNS_LOG_DISABLED
       SNS_DIAG_SERVICE != i &&
#endif
       SNS_SFE_SERVICE != i)
    {
      SNS_ULOG_INIT_SEQ_ERR("%s: service %i failed", __FUNCTION__, i);
      rc = SNS_RC_FAILED;
    }
  }
  return rc;
}

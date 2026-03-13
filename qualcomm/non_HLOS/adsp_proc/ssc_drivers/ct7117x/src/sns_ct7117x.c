/*
*	sns_ct7117x.c
*	Function to register driver library
*
*/


/*==============================================================================
  Include Files
  ============================================================================*/
#include "sns_rc.h"
#include "sns_register.h"
#include "sns_ct7117x_sensor.h"
#include "sns_ct7117x_sensor_instance.h"



/*==============================================================================
  Function Definitions
  ============================================================================*/
/*
*	register snesor_api and instance_api
*/
sns_rc sns_register_ct7117x(sns_register_cb const *register_api)
{ 
  /** Register temperature Sensor. */
  register_api->init_sensor(sizeof(ct7117x_state), &ct7117x_temperature_sensor_api,
      &ct7117x_sensor_instance_api);
  return SNS_RC_SUCCESS;
}

/*
*	sns_ct7117x_sensor_island.c
*	Hardware-specific functions (code and data) for Island mode functions
*
*/

/*==============================================================================
  Include Files
  ============================================================================*/
#include "sns_ct7117x_sensor.h"
#include "sns_ct7117x_hal.h"


/*==============================================================================
  Function Definitions
  ============================================================================*/
/*
*	get sensor uid
*/
static sns_sensor_uid const* ct7117x_temperature_get_sensor_uid(
    sns_sensor const * const this)
{
  ct7117x_state *state = (ct7117x_state*)this->state->state;
  return &state->my_suid;
}

sns_sensor_api ct7117x_temperature_sensor_api =
{
  .struct_len = sizeof(sns_sensor_api),
  .init = &ct7117x_temperature_init,
  .deinit = &ct7117x_temperature_deinit,
  .get_sensor_uid = &ct7117x_temperature_get_sensor_uid,
  .set_client_request = &ct7117x_sensor_set_client_request,
  .notify_event = &ct7117x_sensor_notify_event,
};

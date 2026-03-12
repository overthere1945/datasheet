/*
* sns_himaxdsp_sensor_island.c
* Island 모드 센서 API 정의
*/

/*==============================================================================
  Include Files
  ============================================================================*/
#include "sns_himaxdsp_sensor.h"

/*==============================================================================
  Function Definitions
  ============================================================================*/

/*
* 함수 목적: 센서 UID 반환
*/
static sns_sensor_uid const* himaxdsp_get_sensor_uid(sns_sensor const *const this)
{
    himaxdsp_state *state = (himaxdsp_state*)this->state->state;
    return &state->my_suid;
}

/* 센서 API 정의 */
sns_sensor_api himaxdsp_sensor_api =
{
    .struct_len = sizeof(sns_sensor_api),
    .init = &himaxdsp_init,
    .deinit = &himaxdsp_deinit,
    .get_sensor_uid = &himaxdsp_get_sensor_uid,
    .set_client_request = &himaxdsp_set_client_request,
    .notify_event = &himaxdsp_sensor_notify_event,
};

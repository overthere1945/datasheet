/*
* sns_himaxdsp.c
* HimaxDSP 드라이버 라이브러리 등록 함수
* 이 파일은 Qualcomm 센서 프레임워크(SEE)에 드라이버를 등록하는 진입점 역할을 합니다.
*/

/*==============================================================================
  Include Files
  ============================================================================*/
#include "sns_rc.h"
#include "sns_register.h"
#include "sns_himaxdsp_sensor.h"
#include "sns_himaxdsp_sensor_instance.h"

/*==============================================================================
  Function Definitions
  ============================================================================*/

/*
* 함수 목적: 센서 API와 인스턴스 API를 프레임워크에 등록
* 기능: himaxdsp_state 크기만큼 메모리를 할당하고, 센서 및 인스턴스 동작을 위한 함수 포인터를 등록함
* 입력: register_api - 등록 콜백 함수 포인터
* 출력: sns_rc - 성공 시 SNS_RC_SUCCESS 반환
*/
sns_rc sns_register_himaxdsp(sns_register_cb const *register_api)
{
    /* 센서 및 인스턴스 API 등록 (state 구조체 크기 전달) */
    register_api->init_sensor(sizeof(himaxdsp_state), &himaxdsp_sensor_api,
                              &himaxdsp_sensor_instance_api);
    
    /* 성공 반환 */
    return SNS_RC_SUCCESS;
}

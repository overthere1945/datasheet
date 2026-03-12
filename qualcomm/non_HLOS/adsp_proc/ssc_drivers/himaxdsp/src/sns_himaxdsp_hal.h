#pragma once
/*
* sns_himaxdsp_hal.h
* Hardware Abstraction Layer (HAL) 헤더
* 하드웨어 제어 관련 데이터 타입 및 함수 원형 정의
*/

#include <stdint.h>
#include "sns_sensor.h"
#include "sns_sensor_instance.h"

/* ==========================================================================
   에러 코드 정의
   ========================================================================== */
#define HIMAXDSP_SUCCESS             0
#define HIMAXDSP_ERROR               -1

/* ==========================================================================
   함수 원형 (Function Prototypes)
   ========================================================================== */

/*
* 함수 목적: WHO_AM_I 확인 (HimaxDSP는 레지스터가 없으므로 SPI 통신 테스트로 대체 가능)
*/
sns_rc himaxdsp_check_com_connection(
  sns_sync_com_port_service *scp_service,
  sns_sync_com_port_handle *port_handle);

/*
* 함수 목적: 인터럽트 발생 시 호출되어 데이터를 처리하는 메인 함수
*/
void himaxdsp_handle_interrupt_event(sns_sensor_instance *const instance, sns_time timestamp);

/*
* 함수 목적: SPI를 통해 데이터를 읽어오는 래퍼 함수
*/
sns_rc himaxdsp_com_read_wrapper(
  sns_sync_com_port_service *scp_service,
  sns_sync_com_port_handle *port_handle,
  uint32_t reg_addr, /* SPI에서는 무시됨 (Dummy Write가 될 수 있음) */
  uint8_t  *buffer,
  uint32_t bytes,
  uint32_t *xfer_bytes);

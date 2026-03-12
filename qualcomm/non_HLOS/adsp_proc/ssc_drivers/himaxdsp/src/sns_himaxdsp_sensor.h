#pragma once
/*
* sns_himaxdsp_sensor.h
* 센서 상태 구조체 및 공용 데이터 타입 정의
* 수정사항: 
* 1. sns_printf.h include 추가 (SNS_PRINTF, HIGH 에러 해결) (change-hyungchul)
* 2. himaxdsp_sensor_api extern 선언 유지
*/

#include "sns_sensor.h"
#include "sns_data_stream.h"
#include "sns_sensor_uid.h"
#include "sns_sync_com_port_service.h"
#include "sns_himaxdsp_config.h"
#include "sns_himaxdsp_hal.h"
#include "sns_himaxdsp_sensor_instance.h"

/* change-hyungchul-20260206: SNS_PRINTF 사용을 위한 헤더 추가 */
#include "sns_printf.h"

/* HimaxDSP SUID 정의 (임의의 고유값) */
#define HIMAXDSP_SUID \
{  \
    .sensor_uid =  \
    {  \
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, \
        0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00  \
    }  \
}

/* * change-hyungchul-20260206: Forward Declaration of Sensor API 
 * sns_himaxdsp.c에서 사용하기 위해 extern 선언이 필수입니다.
 */
extern sns_sensor_api himaxdsp_sensor_api;

/* 센서 상태 구조체 (Sensor State) */
typedef struct himaxdsp_state
{
    sns_data_stream           *fw_stream;     /* SUID 조회를 위한 스트림 */
    sns_sensor_uid            irq_suid;       /* 인터럽트 센서 SUID */
    sns_sensor_uid            acp_suid;       /* Async Com Port SUID */
    sns_sensor_uid            my_suid;        /* 자신의 SUID */
    
    /* COM 포트 정보 (인스턴스 생성 전 저장용) */
    sns_com_port_config       com_port_cfg;

    bool                      hw_is_present;  /* 하드웨어 발견 여부 */
    
    /* 서비스 포인터 */
    sns_diag_service          *diag_service;
    sns_sync_com_port_service *scp_service;

} himaxdsp_state;

/* 함수 선언 */
sns_rc himaxdsp_init(sns_sensor *const this);
sns_rc himaxdsp_deinit(sns_sensor *const this);
sns_sensor_instance* himaxdsp_set_client_request(sns_sensor *const this,
                                                struct sns_request const *exist_request,
                                                struct sns_request const *new_request,
                                                bool remove);
sns_rc himaxdsp_sensor_notify_event(sns_sensor *const this);

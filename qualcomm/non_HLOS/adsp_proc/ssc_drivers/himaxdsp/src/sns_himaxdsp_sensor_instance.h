#pragma once
/*
* sns_himaxdsp_sensor_instance.h
* 인스턴스 상태 구조체 정의
* 수정사항: himaxdsp_sensor_instance_api에 대한 extern 선언 추가 (change-hyungchul)
*/

#include <stdint.h>
#include "sns_sensor_instance.h"
#include "sns_data_stream.h"
#include "sns_com_port_types.h"
#include "sns_sync_com_port_service.h"
#include "sns_sensor_uid.h"
#include "sns_async_com_port.pb.h"
#include "sns_math_util.h"
#include "sns_interrupt.pb.h"
#include "sns_std_sensor.pb.h"
#include "sns_physical_sensor_test.pb.h"
#include "sns_diag_service.h"
#include "sns_himaxdsp_config.h"
// change(add)-hyungchul-20260223-0758: Island 모드에서 대용량 SPI 전송 시 Normal 모드로 탈출하기 위한 서비스
#include "sns_island_service.h"

/* * change-hyungchul-20260206: Forward Declaration of Instance API 
 * sns_himaxdsp.c에서 사용하기 위해 extern 선언이 필수입니다.
 */
extern sns_sensor_instance_api himaxdsp_sensor_instance_api;

/* COM 포트 정보 구조체 */
typedef struct himaxdsp_com_port_info
{
    sns_com_port_config        com_config;
    sns_sync_com_port_handle   *port_handle;
} himaxdsp_com_port_info;

/* 인스턴스 상태 구조체 */
typedef struct himaxdsp_instance_state
{
    /* COM 포트 정보 */
    himaxdsp_com_port_info     com_port_info;

    /* SUID 정보 */
    sns_sensor_uid             irq_suid;
    sns_sensor_uid             acp_suid;

    /* 데이터 스트림 */
    sns_data_stream            *interrupt_data_stream;
    sns_data_stream            *async_com_port_data_stream;
    
    // change(add)-hyungchul-20260212-1720: 이미지 청크 전송 시 각 프레임을 식별하기 위한 고유 ID 추적 변수
    uint32_t                   current_frame_id;

    /* 서비스 핸들 */
    sns_diag_service           *diag_service;
    sns_sync_com_port_service  *scp_service;

    // change(add)-hyungchul-20260223-0758: Island -> Normal 모드 전환 요청을 위한 Island 서비스 핸들
    sns_island_service         *island_service;

} himaxdsp_instance_state;

/* 함수 선언 */
sns_rc himaxdsp_inst_init(sns_sensor_instance *const this, sns_sensor_state const *sstate);
sns_rc himaxdsp_inst_deinit(sns_sensor_instance *const this);
sns_rc himaxdsp_inst_set_client_config(sns_sensor_instance *const this, sns_request const *client_request);

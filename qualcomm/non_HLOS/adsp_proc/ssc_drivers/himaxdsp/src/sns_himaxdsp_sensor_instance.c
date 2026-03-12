/*
* sns_himaxdsp_sensor_instance.c
* 센서 인스턴스 초기화 및 설정 관리
* 중요 기능: 인스턴스 초기화 시 인터럽트를 즉시 등록하여 자동 동작 구현.
*/

/*==============================================================================
  Include Files
  ============================================================================*/
#include "sns_mem_util.h"
#include "sns_sensor_instance.h"
#include "sns_service_manager.h"
#include "sns_stream_service.h"
#include "sns_rc.h"
#include "sns_request.h"
#include "sns_types.h"
#include "sns_event_service.h"
#include "sns_pb_util.h"
#include "sns_diag_service.h"
#include "sns_sync_com_port_service.h"
#include "sns_printf.h"
#include "sns_island_service.h"
#include "sns_himaxdsp_hal.h"
#include "sns_himaxdsp_sensor.h"
#include "sns_himaxdsp_sensor_instance.h"
#include "sns_interrupt.pb.h"
#include "sns_async_com_port.pb.h"
#include "pb_encode.h"
#include "pb_decode.h"

/*
 * 함수 목적: 인스턴스 초기화 (최초 1회 실행)
 * 기능: SUID 설정, COM 포트 오픈 및 강제 하드웨어 인터럽트(GPIO 102) 할당
 * 입력: this (인스턴스), sstate (센서 상태)
 * 출력: 없음
 * 리턴: sns_rc 성공 여부
 */
sns_rc himaxdsp_inst_init(sns_sensor_instance *const this,
    sns_sensor_state const *sstate)
{
    himaxdsp_instance_state *state = (himaxdsp_instance_state*)this->state->state;
    himaxdsp_state *sensor_state = (himaxdsp_state*)sstate->state;
    sns_service_manager *service_mgr = this->cb->get_service_manager(this);
    sns_stream_service *stream_mgr = (sns_stream_service*)service_mgr->get_service(service_mgr, SNS_STREAM_SERVICE);

    state->scp_service = (sns_sync_com_port_service*)service_mgr->get_service(service_mgr, SNS_SYNC_COM_PORT_SERVICE);
    state->diag_service = (sns_diag_service*)service_mgr->get_service(service_mgr, SNS_DIAG_SERVICE);

    sns_memscpy(&state->com_port_info.com_config, sizeof(state->com_port_info.com_config),
                &sensor_state->com_port_cfg, sizeof(sensor_state->com_port_cfg));

    state->scp_service->api->sns_scp_register_com_port(&state->com_port_info.com_config,
                                                       &state->com_port_info.port_handle);

    if(NULL == state->com_port_info.port_handle) {
        return SNS_RC_FAILED;
    }

    sns_memscpy(&state->irq_suid, sizeof(state->irq_suid), &sensor_state->irq_suid, sizeof(sensor_state->irq_suid));
    sns_memscpy(&state->acp_suid, sizeof(state->acp_suid), &sensor_state->acp_suid, sizeof(sensor_state->acp_suid));

    state->scp_service->api->sns_scp_open(state->com_port_info.port_handle);
    state->scp_service->api->sns_scp_update_bus_power(state->com_port_info.port_handle, false);

    // change(add)-hyungchul-20260309-1400 시작
    // 설명: 인터럽트(GPIO 102)가 발생하지 않는 문제를 해결하기 위해, 
    // 동료의 코드와 동일하게 초기화 시점에 강제로 인터럽트 핀을 등록합니다.
    if (NULL == state->interrupt_data_stream)
    {
        stream_mgr->api->create_sensor_instance_stream(stream_mgr, this, state->irq_suid, &state->interrupt_data_stream);
    }

    if (NULL != state->interrupt_data_stream)
    {
        uint8_t buffer[20];
        sns_request irq_req = {
            .message_id = SNS_INTERRUPT_MSGID_SNS_INTERRUPT_REQ,
            .request = buffer
        };
        sns_interrupt_req req_payload = sns_interrupt_req_init_default;

        // HimaxDSP 인터럽트 핀 설정 (GPIO 102, Rising Edge)
        req_payload.interrupt_trigger_type = SNS_INTERRUPT_TRIGGER_TYPE_RISING;
        req_payload.interrupt_num = HIMAXDSP_IRQ_NUM; // 102번
        req_payload.interrupt_pull_type = SNS_INTERRUPT_PULL_TYPE_PULL_DOWN;
        req_payload.is_chip_pin = true;
        req_payload.interrupt_drive_strength = SNS_INTERRUPT_DRIVE_STRENGTH_2_MILLI_AMP;

        irq_req.request_len = pb_encode_request(buffer, sizeof(buffer), &req_payload, sns_interrupt_req_fields, NULL);
        if (irq_req.request_len > 0)
        {
            state->interrupt_data_stream->api->send_request(state->interrupt_data_stream, &irq_req);
            SNS_INST_PRINTF(HIGH, this, "Interrupt auto-registered on GPIO %d in Init", req_payload.interrupt_num);
        }
    }
    // change(add)-hyungchul-20260309-1400 끝

    SNS_INST_PRINTF(HIGH, this, "HimaxDSP Instance Init Success");
    return SNS_RC_SUCCESS;
}

/*
 * 함수 목적: 인스턴스 자원 해제
 */
sns_rc himaxdsp_inst_deinit(sns_sensor_instance *const this)
{
    himaxdsp_instance_state *state = (himaxdsp_instance_state*)this->state->state;
    sns_service_manager *service_mgr = this->cb->get_service_manager(this);
    sns_stream_service *stream_mgr = (sns_stream_service*)service_mgr->get_service(service_mgr, SNS_STREAM_SERVICE);

    if(NULL != state->interrupt_data_stream)
        stream_mgr->api->remove_stream(stream_mgr, state->interrupt_data_stream);
    
    if(NULL != state->async_com_port_data_stream)
        stream_mgr->api->remove_stream(stream_mgr, state->async_com_port_data_stream);

    if(NULL != state->scp_service)
    {
        state->scp_service->api->sns_scp_close(state->com_port_info.port_handle);
        state->scp_service->api->sns_scp_deregister_com_port(&state->com_port_info.port_handle);
    }

    return SNS_RC_SUCCESS;
}

/*
 * 함수 목적: 클라이언트 요청 처리
 * 기능: 이미 Init에서 자동 시작되므로, 여기서는 기본적인 로그 출력만 수행
 */
sns_rc himaxdsp_inst_set_client_config(sns_sensor_instance *const this, sns_request const *client_request)
{
    UNUSED_VAR(this);
    UNUSED_VAR(client_request);

    SNS_INST_PRINTF(HIGH, this, "Set Client Config MsgID: %d", client_request->message_id);

    return SNS_RC_SUCCESS;
}

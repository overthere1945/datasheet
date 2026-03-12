/*
* sns_himaxdsp_sensor_instance_island.c
* Island 모드 인스턴스 이벤트 처리
* 기능: Interrupt 스트림에서 이벤트를 수신하여 핸들러(himaxdsp_handle_interrupt_event) 호출
*/

/*==============================================================================
  Include Files
  ============================================================================*/
#include "sns_sensor_instance.h"
#include "sns_sensor_event.h"
#include "sns_himaxdsp_sensor_instance.h"
#include "sns_himaxdsp_hal.h"
#include "sns_interrupt.pb.h"
#include "pb_decode.h"
#include "sns_pb_util.h"

/*==============================================================================
  Function Definitions
  ============================================================================*/

/*
* 함수 목적: 이벤트 알림 처리 (Notify Event)
* 기능: 프레임워크로부터 이벤트를 받아 인터럽트 이벤트인지 확인하고 처리 함수 호출
*/
static sns_rc himaxdsp_inst_notify_event(sns_sensor_instance *const this)
{
    himaxdsp_instance_state *state = (himaxdsp_instance_state*)this->state->state;
    sns_sensor_event *event;

    /* 인터럽트 데이터 스트림 확인 */
    if (state->interrupt_data_stream != NULL)
    {
        event = state->interrupt_data_stream->api->peek_input(state->interrupt_data_stream);
        
        while (NULL != event)
        {
            /* 인터럽트 이벤트인 경우 */
            if (event->message_id == SNS_INTERRUPT_MSGID_SNS_INTERRUPT_EVENT)
            {
                sns_interrupt_event irq_event = sns_interrupt_event_init_default;
                pb_istream_t stream = pb_istream_from_buffer((pb_byte_t*)event->event, event->event_len);
                
                if (pb_decode(&stream, sns_interrupt_event_fields, &irq_event))
                {
                    /* 실제 인터럽트 핸들링 로직 호출 */
                    himaxdsp_handle_interrupt_event(this, irq_event.timestamp);
                }
            }
            /* 다음 이벤트 가져오기 */
            event = state->interrupt_data_stream->api->get_next_input(state->interrupt_data_stream);
        }
    }

    return SNS_RC_SUCCESS;
}

/* 인스턴스 API 정의 (Island 모드용 함수 포인터 매핑) */
sns_sensor_instance_api himaxdsp_sensor_instance_api =
{
    .struct_len = sizeof(sns_sensor_instance_api),
    .init = &himaxdsp_inst_init,
    .deinit = &himaxdsp_inst_deinit,
    .set_client_config = &himaxdsp_inst_set_client_config,
    .notify_event = &himaxdsp_inst_notify_event
};

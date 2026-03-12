/*
 * sns_himaxdsp_sensor.c
 * 센서 생명주기(Init/Deinit) 및 속성(Attribute) 관리
 * 중요 기능: 하드웨어 발견 후 자동으로 인스턴스를 생성하여 App 요청 없이 동작하도록 함.
 */

/*==============================================================================
  Include Files
  ============================================================================*/
#include "sns_mem_util.h"
#include "sns_service_manager.h"
#include "sns_stream_service.h"
#include "sns_service.h"
#include "sns_sensor_util.h"
#include "sns_types.h"
#include "sns_attribute_util.h"
#include "sns_himaxdsp_sensor.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "sns_pb_util.h"
#include "sns_suid.pb.h"

/*==============================================================================
  Function Definitions
  ============================================================================*/

/*
 * 함수 목적: 센서 속성(Attribute) 발행
 * 기능: SUID, 센서 이름, Vendor, Type 등 기본 정보 발행 (불필요한 ODR/Resolution 등 제거)
 */
static void himaxdsp_publish_attributes(sns_sensor *const this)
{
    /* 1. 센서 이름 설정 "HimaxDSP" */
    {
        char const name[] = "HimaxDSP";
        sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
        value.str.funcs.encode = pb_encode_string_cb;
        value.str.arg = &((pb_buffer_arg){ .buf = name, .buf_len = sizeof(name) });
        sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_NAME, &value, 1, false);
    }

    /* 2. 벤더 이름 설정 "LGE-Himax" */
    {
        char const vendor[] = "LGE-Himax";
        sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
        value.str.funcs.encode = pb_encode_string_cb;
        value.str.arg = &((pb_buffer_arg){ .buf = vendor, .buf_len = sizeof(vendor) });
        sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_VENDOR, &value, 1, false);
    }
    
    /* 3. 버전 설정 */
    {
        sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
        value.has_sint = true;
        value.sint = 1;
        sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_VERSION, &value, 1, false);
    }

    /* 4. [change-hyungchul] 센서 타입 설정 "himax_image" (필수) */
    {
        char const type[] = "himax_image";
        sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
        value.str.funcs.encode = pb_encode_string_cb;
        value.str.arg = &((pb_buffer_arg){ .buf = type, .buf_len = sizeof(type) });
        sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_TYPE, &value, 1, false);
    }
    
    /* 5. Stream Type 설정 "on_change" 
    *  이벤트 발생 시에만 데이터를 보내는 방식임을 명시 
    */
    {
        sns_std_sensor_stream_type stream_type = SNS_STD_SENSOR_STREAM_TYPE_ON_CHANGE;
        sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
        value.has_sint = true;
        value.sint = stream_type;
        sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_STREAM_TYPE, &value, 1, false);
    }
    
    /* 6. Rigid Body (물리 센서 여부) */
    {
        sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
        value.has_sint = true;
        value.sint = SNS_STD_SENSOR_RIGID_BODY_TYPE_DISPLAY; // 또는 UNKNOWN
        sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_RIGID_BODY, &value, 1, false);
    }    

    /* 7. 센서 사용 가능 여부 (Available) 설정 */
    {
        sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
        value.has_boolean = true;
        value.boolean = true;
        sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_AVAILABLE, &value, 1, false);
    }

    /* 8. 물리 센서(Physical Sensor) 여부 설정 */
    {
        sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
        value.has_boolean = true;
        value.boolean = true;
        sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_PHYSICAL_SENSOR, &value, 1, true);
    }
    
    /* 9. API Proto 파일 정보 (ssc_sensor_info 출력용)  */
    {
        char const proto_files[] = "sns_himaxdsp_image.proto";

        sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
        value.str.funcs.encode = pb_encode_string_cb;
        value.str.arg = &((pb_buffer_arg){ .buf = proto_files, .buf_len = sizeof(proto_files) });

        /* SNS_STD_SENSOR_ATTRID_API (ID: 12) 속성 발행 */
        sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_API, &value, 1, false);
    }
}

/*
 * 함수 목적: SUID 이벤트 처리
 * 기능: SUID 스트림으로부터 Interrupt, Timer, Async Com Port 등의 SUID를 획득하여 저장
 */
void himaxdsp_sensor_process_suid_events(sns_sensor *const this)
{
    himaxdsp_state *state = (himaxdsp_state*)this->state->state;

    /* 입력된 SUID 이벤트 순회 */
    for(; 0 != state->fw_stream->api->get_input_cnt(state->fw_stream);
            state->fw_stream->api->get_next_input(state->fw_stream))
    {
        sns_sensor_event *event = state->fw_stream->api->peek_input(state->fw_stream);

        if(SNS_SUID_MSGID_SNS_SUID_EVENT == event->message_id)
        {
            pb_istream_t stream = pb_istream_from_buffer((void*)event->event, event->event_len);
            sns_suid_event suid_event = sns_suid_event_init_default;
            pb_buffer_arg data_type_arg = { .buf = NULL, .buf_len = 0 };
            sns_sensor_uid uid_list;
            sns_suid_search suid_search;
            suid_search.suid = &uid_list;
            suid_search.num_of_suids = 0;

            suid_event.data_type.funcs.decode = &pb_decode_string_cb;
            suid_event.data_type.arg = &data_type_arg;
            suid_event.suid.funcs.decode = &pb_decode_suid_event;
            suid_event.suid.arg = &suid_search;

            /* Protobuf 디코딩 */
            if(!pb_decode(&stream, sns_suid_event_fields, &suid_event)) continue;

            if(suid_search.num_of_suids == 0) continue;

            /* 데이터 타입 이름에 따라 SUID 저장 */
            if(0 == strncmp(data_type_arg.buf, "interrupt", data_type_arg.buf_len))
            {
                state->irq_suid = uid_list;
            }
            else if (0 == strncmp(data_type_arg.buf, "async_com_port", data_type_arg.buf_len))
            {
                state->acp_suid = uid_list;
            }
        }
    }
}

/*
 * 함수 목적: SUID 요청 전송
 * 기능: 프레임워크에 특정 데이터 타입(예: interrupt)의 SUID를 요청함
 */
void himaxdsp_send_suid_req(sns_sensor *this, char *const data_type, uint32_t data_type_len)
{
    himaxdsp_state *state = (himaxdsp_state*)this->state->state;
    sns_service_manager *manager = this->cb->get_service_manager(this);
    sns_stream_service *stream_service = (sns_stream_service*)manager->get_service(manager, SNS_STREAM_SERVICE);
    uint8_t buffer[50];
    size_t encoded_len;
    pb_buffer_arg data = (pb_buffer_arg){.buf = data_type, .buf_len = data_type_len};
    sns_suid_req suid_req = sns_suid_req_init_default;

    suid_req.has_register_updates = true;
    suid_req.register_updates = true;
    suid_req.data_type.funcs.encode = &pb_encode_string_cb;
    suid_req.data_type.arg = &data;

    /* SUID 조회를 위한 스트림 생성 (최초 1회) */
    if (state->fw_stream == NULL) {
        stream_service->api->create_sensor_stream(stream_service, this, sns_get_suid_lookup(), &state->fw_stream);
    }

    /* 요청 인코딩 및 전송 */
    encoded_len = pb_encode_request(buffer, sizeof(buffer), &suid_req, sns_suid_req_fields, NULL);
    if (encoded_len > 0) {
        sns_request request = (sns_request){
            .request_len = encoded_len, .request = buffer, .message_id = SNS_SUID_MSGID_SNS_SUID_REQ };
        state->fw_stream->api->send_request(state->fw_stream, &request);
    }
}

/*
 * 함수 목적: 센서 이벤트 알림 처리 (초기화 완료 후 자동 인스턴스 생성 핵심 로직)
 */
sns_rc himaxdsp_sensor_notify_event(sns_sensor *const this)
{
    himaxdsp_state *state = (himaxdsp_state*)this->state->state;

    /* 1. SUID 이벤트 처리 (Dependency 획득) */
    himaxdsp_sensor_process_suid_events(this);

    /* 2. 모든 Dependency SUID를 획득했고, 아직 하드웨어 초기화를 안 했다면 진행 */
    if (!state->hw_is_present &&
            (0 != sns_memcmp(&state->irq_suid, &((sns_sensor_uid){{0}}), sizeof(state->irq_suid))) &&
            (0 != sns_memcmp(&state->acp_suid, &((sns_sensor_uid){{0}}), sizeof(state->acp_suid))))
    {
        /* 하드웨어 발견 간주 (HimaxDSP는 레지스터 체크 없음) */
        state->hw_is_present = true;
        himaxdsp_publish_attributes(this);
        SNS_PRINTF(HIGH, this, "HimaxDSP HW Present. SUIDs found.");

        /* 3. 중요: App 요청 없이도 동작하기 위해 인스턴스를 강제로 자동 생성 */
        /* 이 코드가 없으면 Test App에서 Request를 보내야 인스턴스가 생성됨 */
        sns_sensor_instance *instance = sns_sensor_util_get_shared_instance(this);
        if (NULL == instance)
        {
            instance = this->cb->create_instance(this, sizeof(himaxdsp_instance_state));
            SNS_PRINTF(HIGH, this, "HimaxDSP: Auto-created instance for SPI/Interrupt");
        }
    }

    return SNS_RC_SUCCESS;
}

/*
 * 함수 목적: 센서 초기화 (진입점)
 */
sns_rc himaxdsp_init(sns_sensor *const this)
{
    himaxdsp_state *state = (himaxdsp_state*)this->state->state;
    struct sns_service_manager *smgr = this->cb->get_service_manager(this);

    /* 서비스 획득 */
    state->diag_service = (sns_diag_service *)smgr->get_service(smgr, SNS_DIAG_SERVICE);
    state->scp_service = (sns_sync_com_port_service *)smgr->get_service(smgr, SNS_SYNC_COM_PORT_SERVICE);

    /* SUID 설정 (하드코딩된 값 사용) */
    sns_sensor_uid suid = (sns_sensor_uid)HIMAXDSP_SUID;
    sns_memscpy(&state->my_suid, sizeof(state->my_suid), &suid, sizeof(sns_sensor_uid));

    /* 초기 속성 발행 */
    himaxdsp_publish_attributes(this);

    /* Dependency SUID 요청 (Async Com Port, Interrupt) */
    himaxdsp_send_suid_req(this, "async_com_port", 15);
    himaxdsp_send_suid_req(this, "interrupt", 9);

    SNS_PRINTF(HIGH, this, "HimaxDSP Sensor Init Done");

    return SNS_RC_SUCCESS;
}

sns_rc himaxdsp_deinit(sns_sensor *const this)
{
    UNUSED_VAR(this);
    return SNS_RC_SUCCESS;
}

/*
 * 함수 목적: 클라이언트 요청 처리 (인스턴스 생성/삭제 관리)
 */
sns_sensor_instance *himaxdsp_set_client_request(sns_sensor *const this,
        struct sns_request const *exist_request,
        struct sns_request const *new_request,
        bool remove)
{
    /* 기존 인스턴스 가져오기 */
    sns_sensor_instance *instance = sns_sensor_util_get_shared_instance(this);

    if (remove)
    {
        /* 클라이언트 요청 제거 시 */
        if (NULL != instance)
        {
            instance->cb->remove_client_request(instance, exist_request);
            /* 주의: 자동 실행을 위해 클라이언트가 없어도 인스턴스를 삭제하지 않도록 할 수 있음 */
            /* 여기서는 표준 로직을 따르되, auto-create된 인스턴스는 유지됨 */
        }
    }
    else
    {
        /* 새 요청 발생 시 */
        if (NULL == instance)
        {
            /* 인스턴스가 없으면 생성 (init 호출됨) */
            instance = this->cb->create_instance(this, sizeof(himaxdsp_instance_state));
        }

        /* 요청 추가 */
        if (NULL != instance && NULL != new_request)
        {
            instance->cb->add_client_request(instance, new_request);
        }
    }

    return instance;
}

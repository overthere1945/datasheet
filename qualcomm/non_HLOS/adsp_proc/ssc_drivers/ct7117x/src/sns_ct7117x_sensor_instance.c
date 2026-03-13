/*
 * 목적: ADSP 환경에서 Himax DSP와 SPI 통신을 통해 가변 크기의 JPEG 데이터를 수신하고 공유 메모리에 적재하는 센서 인스턴스 동작 정의
 * 기능:
 * 1. 인터럽트 발생 시 SPI 통신으로 헤더(11 Bytes)를 수신 및 검증
 * 2. 리틀 엔디안으로 JPEG 크기를 파싱하여 필요한 만큼만 Chunk 단위(2KB)로 데이터 수신
 * 3. 수신된 전체 데이터를 Android 앱과 공유된 물리 메모리 공간에 복사
 * 4. 매번 변하는 가짜 온도 데이터를 전송하여 안드로이드의 On-Change 필터링을 우회하고 앱을 깨움
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
#include "sns_time.h"
#include "sns_sensor_event.h"
#include "sns_types.h"
#include "sns_event_service.h"
#include "sns_memmgr.h"
#include "sns_com_port_priv.h"

#include "sns_ct7117x_hal.h"
#include "sns_ct7117x_sensor.h"
#include "sns_ct7117x_sensor_instance.h"

#include "sns_interrupt.pb.h"
#include "sns_async_com_port.pb.h"
#include "sns_timer.pb.h"

#include "pb_encode.h"
#include "pb_decode.h"
#include "sns_pb_util.h"
#include "sns_async_com_port_pb_utils.h"
#include "sns_diag_service.h"
#include "sns_diag.pb.h"
#include "sns_sensor_util.h"
#include "sns_sync_com_port_service.h"
#include "sns_island.h"
#include <qurt.h>
#include <stdio.h>

#define SHARED_PHYS_ADDR  0x81EC0000

// change(add)-hyungchul-20260306-1127 시작
// 설명: 기존 64KB(0x10000)는 128KB의 JPEG를 담기에 부족하여 오버플로우가 발생할 수 있습니다.
// 이를 방지하기 위해 공유 메모리 사이즈를 128KB(0x20000)로 넉넉하게 확장합니다.
#define SHARED_SIZE       0x20000 
// change(add)-hyungchul-20260306-1127 끝

unsigned int myddr_base_addr = 0;
void *psram_virtual_addr = NULL;

/*==============================================================================
  Function Definitions
  ============================================================================*/

/*
 * 목적: 온도 센서 폴링 타이머 시작 함수
 * 기능: 주기적으로 데이터를 읽기 위한 타이머 스트림 생성 및 요청
 * 입력 변수: this (센서 인스턴스 포인터)
 * 출력 변수: 없음
 * 리턴 값: 없음
 */
void ct7117x_start_sensor_temp_polling_timer(sns_sensor_instance *this)
{
    // 인스턴스 상태 구조체 포인터 가져오기
    ct7117x_instance_state *state = (ct7117x_instance_state*)this->state->state;
    // 타이머 설정 요청 페이로드 초기화
    sns_timer_sensor_config req_payload = sns_timer_sensor_config_init_default;
    uint8_t buffer[50] = {0};
    // 타이머 센서 설정을 위한 요청 메시지 구성
    sns_request timer_req = {
        .message_id = SNS_TIMER_MSGID_SNS_TIMER_SENSOR_CONFIG,
        .request    = buffer
    };
    sns_rc rc = SNS_RC_SUCCESS;

    SNS_INST_PRINTF(LOW, this, "ct7117x_start_sensor_temp_polling_timer");

    /*create timer stream*/
    // 타이머 데이터 스트림이 없으면 생성
    if(NULL == state->temperature_timer_data_stream)
    {
        sns_service_manager *smgr = this->cb->get_service_manager(this);
        sns_stream_service *srtm_svc = (sns_stream_service*)smgr->get_service(smgr, SNS_STREAM_SERVICE);
        // 센서 인스턴스 스트림 생성 (타이머 SUID 사용)
        rc = srtm_svc->api->create_sensor_instance_stream(srtm_svc,
                this, state->timer_suid, &state->temperature_timer_data_stream);
    }

    // 스트림 생성 실패 시 에러 로그 출력 및 종료
    if(SNS_RC_SUCCESS != rc
            || NULL == state->temperature_timer_data_stream)
    {
        SNS_INST_PRINTF(ERROR, this, "failed timer stream create rc = %d", rc);
        return;
    }

    /*timer param*/
    // 주기적 타이머 설정
    req_payload.is_periodic = true;
    req_payload.start_time = sns_get_system_time();
    req_payload.timeout_period = state->temperature_info.sampling_intvl;

    // 요청 메시지 인코딩
    timer_req.request_len = pb_encode_request(buffer, sizeof(buffer), &req_payload,
            sns_timer_sensor_config_fields, NULL);
    // 인코딩 성공 시 요청 전송
    if(timer_req.request_len > 0)
    {
        state->temperature_timer_data_stream->api->send_request(state->temperature_timer_data_stream, &timer_req);
        state->temperature_info.timer_is_active = true;
    }
}

/*
 * 목적: 온도 센서 폴링 및 동작 모드 설정 함수
 * 기능: 센서의 상태(DRI 모드 여부 등)에 따라 Normal 또는 Forced 모드로 전환
 * 입력 변수: this (센서 인스턴스 포인터)
 * 출력 변수: 없음
 * 리턴 값: 없음
 */
void ct7117x_set_temperature_config(sns_sensor_instance *const this)
{
    ct7117x_instance_state *state = (ct7117x_instance_state*)this->state->state;

    // 현재 타이머 활성 상태 및 샘플링 간격, DRI 모드 여부 로그 출력
    SNS_INST_PRINTF(LOW, this,
            "temperature_info.timer_is_active:%d state->temperature_info.sampling_intvl:%u is_dri:%d",
            state->temperature_info.timer_is_active,
            state->temperature_info.sampling_intvl, state->is_dri);

    // 샘플링 간격이 설정되어 있는 경우 (동작 중)
    if(state->temperature_info.sampling_intvl > 0)
    {
        if (state->is_dri)
        {
            // DRI 모드인 경우 Normal 모드로 설정 (인터럽트 방식)
            ct7117x_set_power_mode(state, TEMP_NORMAL_MODE);
            // change-20260204-hyungchul: Log added to confirm mode set
            SNS_INST_PRINTF(HIGH, this, "Set Power Mode: NORMAL (DRI Active)");
        }
        else
        {
            // 폴링 모드인 경우 Forced 모드로 설정 후 타이머 시작
            ct7117x_set_power_mode(state, TEMP_FORCED_MODE);
            ct7117x_start_sensor_temp_polling_timer(this);
        }
    }
    else
    {
        // 샘플링 간격이 0인 경우 (측정 중지)
        if (state->is_dri)
        {
            // DRI 모드일 때 별도 처리 없음
        }
        else
        {
            // 폴링 모드일 때 타이머 중지 및 스트림 제거
            state->temperature_info.timer_is_active = false;
            sns_sensor_util_remove_sensor_instance_stream(this, &state->temperature_timer_data_stream);
        }
        // Sleep 모드로 진입
        ct7117x_set_power_mode(state,TEMP_SLEEP_MODE);
        SNS_INST_PRINTF(LOW, this, "Set Power Mode: SLEEP");
    }
}

/*
 * 목적: 하드웨어 재설정 함수
 * 기능: 센서를 재설정하고 동작 모드(고해상도 등)를 재구성
 * 입력 변수: this (인스턴스 포인터), sensor_type (센서 종류)
 * 출력 변수: 없음
 * 리턴 값: 없음
 */
void ct7117x_reconfig_hw(sns_sensor_instance *this,
        ct7117x_sensor_type sensor_type)
{
    ct7117x_instance_state *state = (ct7117x_instance_state*)this->state->state;
    int8_t err = 0;

    SNS_INST_PRINTF(LOW, this, "ct7117x_reconfig_hw state->config_step = %d",state->config_step);
    SNS_INST_PRINTF(LOW, this,
            "enable sensor flag:0x%x publish sensor flag:0x%x",
            state->deploy_info.enable,
            state->deploy_info.publish_sensors);

    // 고해상도 모드로 설정
    err = ct7117x_set_work_mode(state, TEMP_HIGH_RESOLUTION_MODE);
    if (err)
    {
        SNS_INST_PRINTF(ERROR, this, "set oversample failed error = %d", err);
    }

    // 온도 센서 타입인 경우 타이머(또는 인터럽트) 설정 활성화
    if (sensor_type == TEMP_TEMPERATURE)
    {
        ct7117x_set_temperature_config(this);
    }
    /* done with reconfig */
    state->config_step = TEMP_CONFIG_IDLE; 
    SNS_INST_PRINTF(LOW, this, "ct7117x_reconfig_hw finished");
}

/*
 * 목적: 통신 테스트 수행 및 결과 이벤트 발행
 * 기능: WHO_AM_I 값을 바탕으로 센서와의 통신을 검증하여 테스트 결과 이벤트 전달
 * 입력 변수: instance, uid, test_result (통신 성공 여부)
 * 출력 변수: 없음
 * 리턴 값: 없음
 */
static void ct7117x_send_com_test_event(sns_sensor_instance *instance,
        sns_sensor_uid *uid, bool test_result)
{
    uint8_t data[1] = {0};
    pb_buffer_arg buff_arg = (pb_buffer_arg)
    { .buf = &data, .buf_len = sizeof(data) };
    sns_physical_sensor_test_event test_event =
        sns_physical_sensor_test_event_init_default;

    test_event.test_passed = test_result;
    test_event.test_type = SNS_PHYSICAL_SENSOR_TEST_TYPE_COM;
    test_event.test_data.funcs.encode = &pb_encode_string_cb;
    test_event.test_data.arg = &buff_arg;

    // 테스트 결과 이벤트 전송
    pb_send_event(instance,
            sns_physical_sensor_test_event_fields,
            &test_event,
            sns_get_system_time(),
            SNS_PHYSICAL_SENSOR_TEST_MSGID_SNS_PHYSICAL_SENSOR_TEST_EVENT,
            uid);
}


/*
 * 목적: 센서 Self Test 실행 함수
 * 기능: 하드웨어 및 통신 상태를 체크하고 팩토리 테스트 등을 관장함
 * 입력 변수: instance (센서 인스턴스)
 * 출력 변수: 없음
 * 리턴 값: 없음
 */
void ct7117x_run_self_test(sns_sensor_instance *instance)
{
    ct7117x_instance_state *state = (ct7117x_instance_state*)instance->state->state;
    sns_rc rv = SNS_RC_SUCCESS;
    uint8_t buffer = 0;
    bool who_am_i_success = false;

    // WHO_AM_I 레지스터 읽기
    rv = ct7117x_get_who_am_i(state->scp_service,
            state->com_port_info.port_handle,
            &buffer);

    // 읽기 성공 및 값 일치 여부 확인
    if(rv == SNS_RC_SUCCESS
            &&
            buffer == TEMP_WHOAMI_VALUE)
    {
        who_am_i_success = true;
    }

    // 테스트 클라이언트가 존재할 경우 결과 전송
    if(state->temperature_info.test_info.test_client_present)
    {
        if(state->temperature_info.test_info.test_type == SNS_PHYSICAL_SENSOR_TEST_TYPE_COM)
        {
            ct7117x_send_com_test_event(instance, &state->temperature_info.suid, who_am_i_success);
        }
        else if(state->temperature_info.test_info.test_type == SNS_PHYSICAL_SENSOR_TEST_TYPE_FACTORY)
        {
            // Factory test 로직 (필요 시 구현)
        }
        state->temperature_info.test_info.test_client_present = false;
    }
}

qurt_mem_pool_t hwio_pool = 0;
qurt_mem_region_t shared_mem_region = 0;
qurt_mem_region_attr_t hwio_attr;
int qurt_init = 0;
int int_count = 0;

/*
 * 목적: 센서의 하드웨어 인터럽트 처리 핸들러
 * 기능: 공유 메모리를 초기화하고, SPI 통신을 통해 11 Bytes 헤더를 파싱하여 가변 크기 JPEG 데이터를 수신 후 공유 메모리에 적재.
 * 마지막으로 Android 애플리케이션에 수신 완료 이벤트를 전송함.
 * 입력 변수: instance (센서 인스턴스 포인터), timestamp (인터럽트 발생 시간)
 * 출력 변수: 없음
 * 리턴 값: 없음
 */
void ct7117x_handle_interrupt_event(sns_sensor_instance *const instance, sns_time timestamp)
{
    ct7117x_instance_state *state = (ct7117x_instance_state*)instance->state->state;
    SNS_INST_PRINTF(LOW, instance, "interrupt event");
    state->interrupt_timestamp = timestamp;

    uint8_t buffer[2048]; // ADSP 메모리 부족을 방지하기 위해 2KB 버퍼를 사용하여 Chunk 리딩
    uint32_t xfer_bytes = 0;
    uint32_t total_read_bytes = 0;

    // QuRT 공유 메모리 초기화 로직 (기존 구조 유지)
    if(int_count < 30) int_count=int_count+1;
    if(int_count > 20 && qurt_init <= 3)
    {
        if(QURT_EOK == qurt_mem_pool_attach("smem_pool", &hwio_pool))
        {
            qurt_mem_region_attr_init(&hwio_attr);
            qurt_mem_region_attr_set_cache_mode(&hwio_attr, QURT_MEM_CACHE_NONE_SHARED);
            qurt_mem_region_attr_set_mapping(&hwio_attr, QURT_MEM_MAPPING_PHYS_CONTIGUOUS);
            qurt_mem_region_attr_set_physaddr(&hwio_attr, SHARED_PHYS_ADDR);
            qurt_init = 1;

            //hwio_pool
            if (QURT_EOK != qurt_mem_region_create(&shared_mem_region, SHARED_SIZE, hwio_pool, &hwio_attr))
            {
                qurt_init = 2;
            }
            else
            {
                if (QURT_EOK != qurt_mem_region_attr_get(shared_mem_region, &hwio_attr))
                {
                    qurt_init = 3;
                }
                else
                {
                    qurt_mem_region_attr_get_virtaddr(&hwio_attr, &myddr_base_addr);
                    qurt_init = 4;
                }
            }
        }
    }

    if(qurt_init <= 3)
    {
        SNS_INST_PRINTF(HIGH, instance, "qurt_init: %d, int_count: %d", qurt_init, int_count);
    }
    
    {  
        sns_island_exit_internal();
        
        // change(add)-hyungchul-20260306-1127 시작
        // 설명: 무조건 64KB 고정 크기로 읽어오던 비효율적 코드를 제거하고, 
        // 헤더 11바이트 선 파싱 후 리틀엔디안 사이즈를 추출하여 필요한 크기만큼만 동적으로 수신하도록 개선하였습니다.
        
        uint32_t header_len = 11;
        uint32_t jpeg_size = 0;
        uint32_t payload_read_bytes = 0;
        uint8_t *shared_mem_ptr = (uint8_t *)myddr_base_addr;

        // change(add)-hyungchul-20260306-1745 시작
        // 설명: 11바이트만 먼저 읽고 끊으면 Himax DSP의 SPI 통신이 꼬여서 데이터가 오염(Shift)됩니다.
        // 이를 방지하기 위해 원래 동료분의 코드 패턴과 동일하게 첫 번째부터 시원하게 2KB(전체 버퍼 크기)를 한방에 읽어들입니다.
        uint32_t first_chunk = sizeof(buffer); // 2048
        state->com_read(state->scp_service, state->com_port_info.port_handle,
                        0x00, buffer, first_chunk, &xfer_bytes);
        total_read_bytes += xfer_bytes;

        // 2. 첫 청크에서 헤더 검증 (SYNC: 0xC0 0x5A, Type: 0x01)
        if(buffer[0] == 0xC0 && buffer[1] == 0x5A && buffer[2] == 0x01)
        {
            // 리틀 엔디안 방식으로 파싱: [3]이 최하위, [6]이 최상위 바이트
            jpeg_size = (buffer[6] << 24) | (buffer[5] << 16) | (buffer[4] << 8) | buffer[3];
            SNS_INST_PRINTF(HIGH, instance, "Header Parsed successfully. JPEG Size: %u bytes", jpeg_size);

            // 안정성 검사: 수신해야 할 헤더+페이로드 크기가 공유 메모리 최대 사이즈를 초과하는지 검증
            if((header_len + jpeg_size) > SHARED_SIZE)
            {
                SNS_INST_PRINTF(ERROR, instance, "Error: JPEG Size exceeds Shared Memory Capacity!");
                jpeg_size = SHARED_SIZE - header_len; // 메모리 덮어쓰기 크래시를 방지하기 위해 강제 절단
            }

            // Android C++ 스레드가 아직 다 쓰지 않은 데이터를 읽어가는 Race Condition을 방지하기 위해,
            // 공유 메모리에 쓰기 시작할 때는 헤더의 첫 번째 바이트(Sync)를 0x00으로 지워서 복사합니다.
            if(qurt_init == 4 && shared_mem_ptr != NULL)
            {
                buffer[0] = 0x00; // Flag Down (물리 메모리에 쓰기 전 조작)
                memcpy(shared_mem_ptr, buffer, xfer_bytes);
            }

            // 처음 2KB를 읽었으므로, 헤더(11바이트)를 제외한 나머지를 '이미 읽은 본문 크기'에 누적합니다.
            if (xfer_bytes > header_len) {
                payload_read_bytes += (xfer_bytes - header_len);
            }

            // 3. JPEG 이미지 남은 본문 데이터 수신 (2KB Chunk 유지)
            while(payload_read_bytes < jpeg_size)
            {
                uint32_t remain = jpeg_size - payload_read_bytes;
                uint32_t chunk = remain > sizeof(buffer) ? sizeof(buffer) : remain;
                
                state->com_read(state->scp_service, state->com_port_info.port_handle,
                                0x00, buffer, chunk, &xfer_bytes);

                // 읽은 Chunk 데이터를 헤더 이후의 오프셋부터 공유 메모리에 이어서 기록
                if(qurt_init == 4 && shared_mem_ptr != NULL)
                {
                    void* current_dest = (void*)(shared_mem_ptr + header_len + payload_read_bytes);
                    memcpy(current_dest, buffer, xfer_bytes);
                }
                
                payload_read_bytes += xfer_bytes;
                total_read_bytes += xfer_bytes;
            }

            qurt_mem_barrier(); // 메모리 동기화 보장

            // change(add)-hyungchul-20260306-1610 시작
            // 설명: 본문 데이터 복사가 완벽하게 끝났으므로, 첫 번째 바이트를 다시 0xC0으로 세팅합니다.
            // 안드로이드 측 백그라운드 스레드는 이 값을 확인하고 즉시 이미지를 파일로 빼내게 됩니다.
            // (Sensor HAL이 막혀서 작동하지 않는 기존 Dummy 이벤트 전송 로직은 삭제했습니다)
            if(qurt_init == 4 && shared_mem_ptr != NULL)
            {
                shared_mem_ptr[0] = 0xC0; // Flag Up
            }
            // change(add)-hyungchul-20260306-1610 끝
        }
        else
        {
            // 헤더의 Sync 바이트가 일치하지 않는 경우
            SNS_INST_PRINTF(ERROR, instance, "Header Sync Failed! Found: [%02X %02X %02X]", buffer[0], buffer[1], buffer[2]);
        }
        // change(add)-hyungchul-20260306-1127 끝
    }
    SNS_INST_PRINTF(HIGH, instance, "Total read bytes (Header + Payload): %u", total_read_bytes);
}

/*
 * 목적: 물리적 센서 설정 이벤트 전송
 * 기능: 정상 동작 모드나 ODR, Range 등 센서의 설정을 반영하여 Framework로 이벤트 전달
 * 입력 변수: instance
 * 출력 변수: 없음
 * 리턴 값: 없음
 */
void ct7117x_send_config_event(sns_sensor_instance *const instance)
{
    ct7117x_instance_state *state = (ct7117x_instance_state*)instance->state->state;
    sns_std_sensor_physical_config_event phy_sensor_config =
        sns_std_sensor_physical_config_event_init_default;
    char operating_mode[] = "NORMAL";
    pb_buffer_arg op_mode_args;

    /*op mode*/
    op_mode_args.buf = &operating_mode[0];
    op_mode_args.buf_len = sizeof(operating_mode);

    /*physical sensor config*/
    phy_sensor_config.has_sample_rate = true;
    phy_sensor_config.has_water_mark = true;
    phy_sensor_config.water_mark = 1;
    phy_sensor_config.operation_mode.funcs.encode = &pb_encode_string_cb;
    phy_sensor_config.operation_mode.arg = &op_mode_args;
    phy_sensor_config.has_active_current = true;
    phy_sensor_config.has_resolution = true;
    phy_sensor_config.range_count = 2;
    phy_sensor_config.stream_is_synchronous = false;
    phy_sensor_config.has_dri_enabled= true;
    phy_sensor_config.dri_enabled=false;

    // 온도 센서에 대한 설정 이벤트 전송
    if(state->deploy_info.publish_sensors & TEMP_TEMPERATURE)
    {
        phy_sensor_config.sample_rate = state->temperature_info.sampling_rate_hz;
        phy_sensor_config.has_active_current = true;
        phy_sensor_config.active_current = 3;
        phy_sensor_config.resolution = CT7117X_TEMPERATURE_RESOLUTION;
        phy_sensor_config.range_count = 2;
        phy_sensor_config.range[0] = CT7117X_TEMPERATURE_RANGE_MIN;
        phy_sensor_config.range[1] = CT7117X_TEMPERATURE_RANGE_MAX;

        pb_send_event(
                instance,
                sns_std_sensor_physical_config_event_fields,
                &phy_sensor_config,
                sns_get_system_time(),
                SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_PHYSICAL_CONFIG_EVENT,
                &state->temperature_info.suid);
    }
}

/*
 * 목적: Output Data Rate (ODR) 유효성 검사 및 제한
 * 기능: 허용된 ODR 범위를 넘을 경우 강제 조정하거나 중지 처리
 * 입력 변수: this
 * 출력 변수: 없음
 * 리턴 값: sns_rc (지원 여부에 따른 결과값)
 */
static sns_rc ct7117x_validate_sensor_temp_odr(sns_sensor_instance *this)
{
    sns_rc rc = SNS_RC_SUCCESS;
    ct7117x_instance_state *state = (ct7117x_instance_state*)this->state->state;
    SNS_INST_PRINTF(LOW, this, "temperature odr = %d", (int8_t)state->temperature_info.sampling_rate_hz);

    // ODR 범위에 따라 설정값 조정
    if(state->temperature_info.sampling_rate_hz > TEMP_ODR_0
            &&
            state->temperature_info.sampling_rate_hz <= TEMP_ODR_1)
    {
        state->temperature_info.sampling_rate_hz = TEMP_ODR_1;
    }
    else if(state->temperature_info.sampling_rate_hz > TEMP_ODR_1)
    {
        state->temperature_info.sampling_rate_hz = TEMP_ODR_5;
    }
    /*ODR < 0*/
    else
    {
        // ODR이 0 이하면 측정 중지
        state->temperature_info.sampling_intvl = 0;
        state->temperature_info.timer_is_active = 0;
        SNS_INST_PRINTF(LOW, this, "close temperature sensor = %d, timer_is_active =%d",
                (uint32_t)state->temperature_info.sampling_rate_hz, state->temperature_info.timer_is_active);
        rc = SNS_RC_NOT_SUPPORTED;
    }

    // 성공 시 샘플링 간격(Tick) 계산
    if (rc == SNS_RC_SUCCESS)
    {
        state->temperature_info.sampling_intvl =
            sns_convert_ns_to_ticks(1000000000.0 / state->temperature_info.sampling_rate_hz);
        SNS_INST_PRINTF(LOW, this, "temperature timer_value = %u", (uint32_t)state->temperature_info.sampling_intvl);
    }

    return rc;
}

/*
 * 목적: 센서 인스턴스가 사용하던 리소스 정리
 * 기능: 할당된 스트림 서비스들을 메모리에서 해제하고 통신 포트 클로즈
 * 입력 변수: state, stream_mgr
 * 출력 변수: 없음
 * 리턴 값: 없음
 */
static void inst_cleanup(ct7117x_instance_state *state, sns_stream_service *stream_mgr)
{

    // 비동기 COM 포트 스트림 제거
    if(NULL != state->async_com_port_data_stream)
    {
        stream_mgr->api->remove_stream(stream_mgr, state->async_com_port_data_stream);
        state->async_com_port_data_stream = NULL;
    }
    // 타이머 데이터 스트림 제거
    if(NULL != state->temperature_timer_data_stream)
    {
        stream_mgr->api->remove_stream(stream_mgr, state->temperature_timer_data_stream);
        state->temperature_timer_data_stream = NULL;
    }
    // 인터럽트 데이터 스트림 제거
    if(NULL != state->interrupt_data_stream)
    {
        stream_mgr->api->remove_stream(stream_mgr, state->interrupt_data_stream);
        state->interrupt_data_stream = NULL;
    }
    // SCP 서비스 해제 및 포트 닫기
    if(NULL != state->scp_service)
    {
        state->scp_service->api->sns_scp_close(state->com_port_info.port_handle);
        state->scp_service->api->sns_scp_deregister_com_port(&state->com_port_info.port_handle);
        state->scp_service = NULL;
    }
}

/*
 * 목적: 센서 인스턴스의 초기화 처리
 * 기능: SUID 설정, 통신 포트 구성, 상태 정보 복사 및 인터럽트 강제 할당 (GPIO 102)
 * 입력 변수: this, sstate
 * 출력 변수: 없음
 * 리턴 값: sns_rc (초기화 성공 여부)
 */
sns_rc ct7117x_temp_inst_init(sns_sensor_instance * const this,
        sns_sensor_state const *sstate)
{
    ct7117x_instance_state *state =
        (ct7117x_instance_state*) this->state->state;
    ct7117x_state *sensor_state =
        (ct7117x_state*) sstate->state;
    float stream_data[1] = {0};
    sns_service_manager *service_mgr = this->cb->get_service_manager(this);
    sns_stream_service *stream_mgr = (sns_stream_service*)
        service_mgr->get_service(service_mgr, SNS_STREAM_SERVICE);

    uint64_t buffer[10];
    pb_ostream_t stream = pb_ostream_from_buffer((pb_byte_t *)buffer, sizeof(buffer));
    sns_diag_batch_sample batch_sample = sns_diag_batch_sample_init_default;
    uint8_t arr_index = 0;
    float diag_temp[TEMP_NUM_AXES];
    pb_float_arr_arg arg = {.arr = (float*)diag_temp, .arr_len = TEMP_NUM_AXES,
        .arr_index = &arr_index};
    batch_sample.sample.funcs.encode = &pb_encode_float_arr_cb;
    batch_sample.sample.arg = &arg;

    // Sync Com Port 서비스 가져오기
    state->scp_service = (sns_sync_com_port_service*)
        service_mgr->get_service(service_mgr, SNS_SYNC_COM_PORT_SERVICE);

    SNS_INST_PRINTF(LOW, this, "<sns_see_if__  init> from sensor:0x%x", sensor_state->sensor);

    /**---------Setup stream connections with dependent Sensors---------*/
    // Async Com Port 스트림 생성
    stream_mgr->api->create_sensor_instance_stream(stream_mgr,
            this,
            sensor_state->acp_suid,
            &state->async_com_port_data_stream);

    /*Initialize COM port to be used by the Instance */
    // COM 포트 설정 복사 및 등록
    sns_memscpy(&state->com_port_info.com_config,
            sizeof(state->com_port_info.com_config),
            &sensor_state->com_port_info.com_config,
            sizeof(sensor_state->com_port_info.com_config));

    state->scp_service->api->sns_scp_register_com_port(&state->com_port_info.com_config,
            &state->com_port_info.port_handle);

    // 스트림 생성 또는 포트 등록 실패 시 정리 후 종료
    if(NULL == state->async_com_port_data_stream ||
            NULL == state->com_port_info.port_handle)
    {
        inst_cleanup(state, stream_mgr);
        return SNS_RC_FAILED;
    }

    /**----------- Copy all Sensor UIDs in instance state -------------*/
    // SUID 정보 복사
    sns_memscpy(&state->temperature_info.suid,
            sizeof(state->temperature_info.suid),
            &sensor_state->my_suid,
            sizeof(state->temperature_info.suid));
    sns_memscpy(&state->timer_suid,
            sizeof(state->timer_suid),
            &sensor_state->timer_suid,
            sizeof(sensor_state->timer_suid));
    sns_memscpy(&state->irq_suid,
            sizeof(state->irq_suid),
            &sensor_state->irq_suid,
            sizeof(sensor_state->irq_suid));
    state->irq_num = 102; // 인터럽트 핀 번호 설정

    /** Copy calibration data*/
    // 캘리브레이션 데이터 복사
    sns_memscpy(&state->calib_param,
            sizeof(state->calib_param),
            &sensor_state->calib_param,
            sizeof(sensor_state->calib_param));
    state->interface = sensor_state->com_port_info.com_config.bus_instance;

    /* change-20260204-hyungchul: Force DRI enabled in init for auto-start */
    /* 기본적으로 is_dri를 true로 강제 설정하여 인터럽트 모드로 동작하게 함 */
    state->is_dri = true; // sensor_state->is_dri; 기존 코드 주석 처리하고 강제 true

    state->op_mode = FORCED_MODE;

    /* com read function*/
    state->com_read = ct7117x_com_read_wrapper;
    /*com write function*/
    state->com_write = ct7117x_com_write_wrapper;

    /* set the data report length to the framework */
    state->encoded_imu_event_len = pb_get_encoded_size_sensor_stream_event(stream_data, 1);

    state->diag_service =  (sns_diag_service*)
        service_mgr->get_service(service_mgr, SNS_DIAG_SERVICE);

    state->scp_service =  (sns_sync_com_port_service*)
        service_mgr->get_service(service_mgr, SNS_SYNC_COM_PORT_SERVICE);

    // COM 포트 오픈
    state->scp_service->api->sns_scp_open(state->com_port_info.port_handle);

    // 버스 파워 끄기 (대기 상태)
    state->scp_service->api->sns_scp_update_bus_power(state->com_port_info.port_handle,
            false);


    /** Configure the Async Com Port */
    {
        // Async Com Port 설정 요청 전송
        sns_data_stream* data_stream = state->async_com_port_data_stream;
        sns_com_port_config* com_config = &sensor_state->com_port_info.com_config;
        uint8_t pb_encode_buffer[100];
        sns_request async_com_port_request =
        {
            .message_id  = SNS_ASYNC_COM_PORT_MSGID_SNS_ASYNC_COM_PORT_CONFIG,
            .request     = &pb_encode_buffer
        };

        state->ascp_config.bus_type          = (com_config->bus_type == SNS_BUS_I2C) ?
            SNS_ASYNC_COM_PORT_BUS_TYPE_I2C : SNS_ASYNC_COM_PORT_BUS_TYPE_SPI;
        state->ascp_config.slave_control     = com_config->slave_control;
        state->ascp_config.reg_addr_type     = SNS_ASYNC_COM_PORT_REG_ADDR_TYPE_8_BIT;
        state->ascp_config.min_bus_speed_kHz = com_config->min_bus_speed_KHz;
        state->ascp_config.max_bus_speed_kHz = com_config->max_bus_speed_KHz;
        state->ascp_config.bus_instance      = com_config->bus_instance;

        async_com_port_request.request_len =
            pb_encode_request(pb_encode_buffer,
                    sizeof(pb_encode_buffer),
                    &state->ascp_config,
                    sns_async_com_port_config_fields,
                    NULL);
        data_stream->api->send_request(data_stream, &async_com_port_request);
    }

    /** Determine size of sns_diag_sensor_state_raw as defined in
     * sns_diag.proto
     * 로그 패킷 크기 계산
     */
    if(pb_encode_tag(&stream, PB_WT_STRING,
                sns_diag_sensor_state_raw_sample_tag))
    {
        if(pb_encode_delimited(&stream, sns_diag_batch_sample_fields,
                    &batch_sample))
        {
            state->log_raw_encoded_size = stream.bytes_written;
        }
    }

    /* change-20260204-hyungchul: Register Interrupt immediately during init */
    /* 초기화 시점에 바로 인터럽트를 등록하여 Test App 요청 없이도 동작하게 수정 */
    if (state->is_dri)
    {

        // 인터럽트 스트림 생성
        if (NULL == state->interrupt_data_stream)
        {
            sns_service_manager *smgr = this->cb->get_service_manager(this);
            sns_stream_service *stream_svc = (sns_stream_service*)smgr->get_service(smgr, SNS_STREAM_SERVICE);
            stream_svc->api->create_sensor_instance_stream(stream_svc, this, state->irq_suid, &state->interrupt_data_stream);
        }

        // 인터럽트 요청 전송
        if (NULL != state->interrupt_data_stream)
        {
            uint8_t buffer[20];
            sns_request irq_req = {
                .message_id = SNS_INTERRUPT_MSGID_SNS_INTERRUPT_REQ,
                .request = buffer
            };
            sns_interrupt_req req_payload = sns_interrupt_req_init_default;

            // 사용자 요청에 따라 GPIO 102, Rising Edge 설정
            req_payload.interrupt_trigger_type = SNS_INTERRUPT_TRIGGER_TYPE_RISING;
            req_payload.interrupt_num = 102; // GPIO_102
            req_payload.interrupt_pull_type = SNS_INTERRUPT_PULL_TYPE_PULL_DOWN;
            req_payload.is_chip_pin = true;
            req_payload.interrupt_drive_strength = SNS_INTERRUPT_DRIVE_STRENGTH_2_MILLI_AMP;

            irq_req.request_len = pb_encode_request(buffer, sizeof(buffer), &req_payload, sns_interrupt_req_fields, NULL);
            if (irq_req.request_len > 0)
            {
                state->interrupt_data_stream->api->send_request(state->interrupt_data_stream, &irq_req);
                SNS_INST_PRINTF(HIGH, this, "change-20260204-hyungchul: Interrupt auto-registered on pin %d in init", req_payload.interrupt_num);
            }
        }
    }

    /* change-20260204-hyungchul: Force default ODR and activate sensor in init */
    /* 초기화 시 강제로 기본 ODR(5Hz)을 설정하고 센서를 활성화시킴 */
    state->temperature_info.sampling_rate_hz = TEMP_ODR_5;
    state->temperature_info.sampling_intvl = sns_convert_ns_to_ticks(1000000000.0 / state->temperature_info.sampling_rate_hz);

    // 강제로 Config 함수를 호출하여 Normal Mode(DRI)로 설정
    ct7117x_set_temperature_config(this);
    SNS_INST_PRINTF(HIGH, this, "change-20260204-hyungchul: Auto-started sensor with ODR 5Hz");

    SNS_INST_PRINTF(LOW, this, "<sns_see_if__ init> success");
    return SNS_RC_SUCCESS;
}

/*
 * 목적: 센서 인스턴스 해제 함수
 * 기능: 종료 시 인스턴스 및 스트림 해제 작업 수행
 * 입력 변수: this
 * 출력 변수: 없음
 * 리턴 값: sns_rc
 */
sns_rc ct7117x_temp_inst_deinit(sns_sensor_instance *const this)
{
    ct7117x_instance_state *state = (ct7117x_instance_state*) this->state->state;
    sns_service_manager *service_mgr = this->cb->get_service_manager(this);
    sns_stream_service *stream_mgr = (sns_stream_service*) service_mgr->get_service(service_mgr, SNS_STREAM_SERVICE);
    inst_cleanup(state, stream_mgr);
    return SNS_RC_SUCCESS;
}

/*
 * 목적: 클라이언트 설정 요청 처리 함수
 * 기능: 상위 앱 혹은 Test앱에서 내려오는 ODR 설정, 통신 설정 등의 커맨드를 처리하고 센서에 반영함
 * 입력 변수: this, client_request (클라이언트가 보낸 요청 구조체)
 * 출력 변수: 없음
 * 리턴 값: sns_rc (설정 반영 결과)
 */
sns_rc ct7117x_temp_inst_set_client_config(
        sns_sensor_instance * const this,
        sns_request const *client_request)
{
    ct7117x_instance_state *state = (ct7117x_instance_state*) this->state->state;

    if(client_request->message_id != SNS_STD_MSGID_SNS_STD_FLUSH_REQ)
    {
        SNS_INST_PRINTF(MED, this, "[%u] client_config: temp=%u",
                state->temperature_info, client_request->message_id);
    }
    SNS_INST_PRINTF(HIGH, this, "set_client_config: temp=%u", client_request->message_id);

    state->client_req_id = client_request->message_id;
    float desired_sample_rate = 0;
    float desired_report_rate = 0;
    ct7117x_sensor_type sensor_type = TEMP_SENSOR_INVALID;
    // ct7117x_power_mode op_mode = INVALID_WORK_MODE;
    sns_temp_cfg_req *payload = (sns_temp_cfg_req*)client_request->request;
    sns_rc rv = SNS_RC_SUCCESS;
    bool temp_odr_change = false;
    sns_service_manager *mgr = this->cb->get_service_manager(this);
    sns_event_service *event_service = (sns_event_service*)mgr->get_service(mgr, SNS_EVENT_SERVICE);

    SNS_INST_PRINTF(LOW, this, "<sns_see_if__  set_client_config>");

    /* Turn COM port ON, *physical* */
    state->scp_service->api->sns_scp_update_bus_power(state->com_port_info.port_handle, true);

    if (client_request->message_id == SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_CONFIG) 
    {
        // 1. Extract sample, report rates from client_request.
        // 2. Configure sensor HW.
        // 3. sendRequest() for Timer to start/stop in case of polling using timer_data_stream.
        // 4. sendRequest() for Intrerupt register/de-register in case of DRI using interrupt_data_stream.
        // 5. Save the current config information like type, sample_rate, report_rate, etc.
        
        if (client_request->message_id == SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_CONFIG) 
        {
            desired_sample_rate = payload->sample_rate;
            desired_report_rate = payload->report_rate;
            sensor_type = payload->sensor_type;
            //op_mode = payload->op_mode;
        }
        
        if (desired_report_rate > desired_sample_rate) 
        {
            /* bad request. Return error or default report_rate to sample_rate */
            desired_report_rate = desired_sample_rate;
        }
        
        if (sensor_type == TEMP_TEMPERATURE) 
        {
            /* change-20260204-hyungchul: Ignore ODR 0 request and Force 5Hz */
            /* Framework가 클라이언트가 없다고 판단하여 ODR 0(종료)을 보내도, 
               테스트를 위해 이를 무시하고 5Hz로 강제 설정하여 센서를 계속 살려둠 */
            if (desired_sample_rate == 0.0f) 
            {
                SNS_INST_PRINTF(HIGH, this, "change-20260204-hyungchul: Force ODR 5Hz to prevent shutdown (Ignored ODR 0)");
                desired_sample_rate = TEMP_ODR_5;
                // 상태 변수 강제 업데이트 (Validate 함수가 이 값을 참조하므로 필수)
                state->temperature_info.sampling_rate_hz = desired_sample_rate;
                // 센서 타입도 유효하게 설정
                sensor_type = TEMP_TEMPERATURE;
            }

            sns_time temp_interval = (uint32_t)state->temperature_info.sampling_intvl;
            rv = ct7117x_validate_sensor_temp_odr(this);
            SNS_INST_PRINTF(LOW, this, "temperature temp_interval = %u,state->temperature_info.sampling_intvl= %u", (uint32_t)temp_interval, (uint32_t)state->temperature_info.sampling_intvl);
            
            if(temp_interval != state->temperature_info.sampling_intvl)
            {
                temp_odr_change = true;
                SNS_INST_PRINTF(LOW, this, "odr_change!!!!");
            }
            
            if(rv != SNS_RC_SUCCESS && desired_sample_rate != 0)
            {
                // TODO Unsupported rate. Report error using sns_std_error_event.
                SNS_INST_PRINTF(ERROR, this, "sensor_temp ODR match error %d", rv);
                sns_sensor_event *event = event_service->api->alloc_event(event_service, this, 0);
                if(NULL != event)
                {
                    event->message_id = SNS_STD_MSGID_SNS_STD_ERROR_EVENT;
                    event->event_len = 0;
                    event->timestamp = sns_get_system_time();
                    event_service->api->publish_event(event_service, this, event,
                            &state->temperature_info.suid);
                }
                //return rv;
            }
        }
        
        if(true == temp_odr_change)
        {
            ct7117x_set_temperature_config(this);
        }

        //   if (state->config_step == TEMP_CONFIG_IDLE) 
        //	{
        //	SNS_INST_PRINTF(ERROR, this, "call config function");                 //0122
        //      ct7117x_reconfig_hw(this, sensor_type);
        //    }

        ct7117x_send_config_event(this);
    }
    else if (client_request->message_id == SNS_PHYSICAL_SENSOR_TEST_MSGID_SNS_PHYSICAL_SENSOR_TEST_CONFIG) 
    {
        // ct7117x_run_self_test(this); // Original COM test.

        bool is_irq_suid_valid = (0 != sns_memcmp(&state->irq_suid, &((sns_sensor_uid){{0}}), sizeof(state->irq_suid)));

        // Per request, register interrupt on self-test request.
        if (NULL == state->interrupt_data_stream && is_irq_suid_valid)
        {
            sns_service_manager *smgr = this->cb->get_service_manager(this);
            sns_stream_service *stream_svc = (sns_stream_service*)smgr->get_service(smgr, SNS_STREAM_SERVICE);
            stream_svc->api->create_sensor_instance_stream(stream_svc, this, state->irq_suid, &state->interrupt_data_stream);
        }

        if (NULL != state->interrupt_data_stream)
        {
            uint8_t buffer[20];
            sns_request irq_req = {
                .message_id = SNS_INTERRUPT_MSGID_SNS_INTERRUPT_REQ,
                .request = buffer
            };
            sns_interrupt_req req_payload = sns_interrupt_req_init_default;
            req_payload.interrupt_trigger_type = SNS_INTERRUPT_TRIGGER_TYPE_RISING;
            req_payload.interrupt_num = 102;
            req_payload.interrupt_pull_type = SNS_INTERRUPT_PULL_TYPE_PULL_DOWN;
            req_payload.is_chip_pin = true;
            req_payload.interrupt_drive_strength = SNS_INTERRUPT_DRIVE_STRENGTH_2_MILLI_AMP;

            irq_req.request_len = pb_encode_request(buffer, sizeof(buffer), &req_payload, sns_interrupt_req_fields, NULL);
            if (irq_req.request_len > 0)
            {
                state->interrupt_data_stream->api->send_request(state->interrupt_data_stream, &irq_req);
                SNS_INST_PRINTF(HIGH, this, "Interrupt registered for self-test on pin %d", req_payload.interrupt_num);
            }
        }
        else
        {
            SNS_INST_PRINTF(ERROR, this, "Failed to create interrupt stream for self-test");
        }
        
        state->new_self_test_request = false;
    }
    
    // Turn COM port OFF
    state->scp_service->api->sns_scp_update_bus_power(state->com_port_info.port_handle, false);

    SNS_INST_PRINTF(LOW, this, "<sns_see_if__  set_client_config> exit");
    return SNS_RC_SUCCESS;
}

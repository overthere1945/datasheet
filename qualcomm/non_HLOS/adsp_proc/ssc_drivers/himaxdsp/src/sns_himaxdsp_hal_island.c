/*
 * 목적: HimaxDSP 센서의 Island 모드(Low Power) 하드웨어 인터럽트 및 SPI 통신 처리
 * 기능: 
 * 1. GPIO 인터럽트 수신 시 SPI로 2KB씩 이미지를 읽어옴.
 * 2. 기존의 무거운 Protobuf(QMI) 전송 방식을 폐기하고 UIO 공유 메모리(0x81EC0000)를 활용.
 * 3. Race Condition 방지를 위해 메모리 맨 앞(Index 0)에 0x00(쓰기 중) / 0xC0(쓰기 완료) 플래그를 세팅함.
 */

#include "sns_rc.h"
#include "sns_time.h"
#include "sns_sensor_event.h"
#include "sns_event_service.h"
#include "sns_mem_util.h"
#include "sns_math_util.h"
#include "sns_service_manager.h"
#include "sns_com_port_types.h"
#include "sns_sync_com_port_service.h"
#include "sns_printf.h"
#include "sns_himaxdsp_hal.h"
#include "sns_himaxdsp_sensor_instance.h"
#include "sns_himaxdsp_config.h"
#include "sns_himaxdsp_sensor.h"
#include "pb_encode.h"
#include "sns_pb_util.h"

// 컴파일러의 함수 원형 검사를 통과하기 위해 명시적으로 extern 선언을 추가합니다.
extern void sns_island_exit_internal(void);

// UIO 공유 메모리 제어를 위한 QuRT RTOS 헤더 및 주소 정의
#include <qurt.h>

#define SHARED_PHYS_ADDR  0x81EC0000 // UIO로 매핑될 커널 물리 주소
#define SHARED_SIZE       0x20000    // 최대 128KB 할당

// QuRT 메모리 관리를 위한 전역 변수들
qurt_mem_pool_t hwio_pool = 0;              // 라인 설명: 메모리 풀 핸들
qurt_mem_region_t shared_mem_region = 0;    // 라인 설명: 생성된 공유 메모리 영역 핸들
qurt_mem_region_attr_t hwio_attr;           // 라인 설명: 메모리 속성
unsigned int myddr_base_addr = 0;           // 라인 설명: ADSP 내부에 매핑된 가상 주소
int qurt_init = 0;                          // 라인 설명: 초기화 단계 상태값
int int_count = 0;                          // 라인 설명: 부팅 초기 딜레이용 카운터

/*
 * 함수 목적: SPI 통신 가능 여부(더미) 확인
 * 입력: scp_service, port_handle
 * 출력: 없음
 * 리턴: sns_rc (항상 SUCCESS 반환)
 */
sns_rc himaxdsp_check_com_connection(
    sns_sync_com_port_service *scp_service,
    sns_sync_com_port_handle *port_handle)
{
    UNUSED_VAR(scp_service);
    UNUSED_VAR(port_handle);
    return SNS_RC_SUCCESS;
}

/*
 * 함수 목적: SPI 통신 읽기 래퍼 함수
 * 입력: scp_service, port_handle, reg_addr(무시됨), buffer(저장소), bytes(읽을 크기), xfer_bytes(실제 읽은 크기)
 * 출력: buffer, xfer_bytes
 * 리턴: sns_rc 통신 결과
 */
sns_rc himaxdsp_com_read_wrapper(
    sns_sync_com_port_service *scp_service,
    sns_sync_com_port_handle *port_handle,
    uint32_t reg_addr,
    uint8_t *buffer,
    uint32_t bytes,
    uint32_t *xfer_bytes)
{
    sns_port_vector port_vec;
    port_vec.buffer = buffer;
    port_vec.bytes = bytes;
    port_vec.is_write = false;
    port_vec.reg_addr = reg_addr;

    sns_rc rc = scp_service->api->sns_scp_register_rw(port_handle, &port_vec, 1, false, xfer_bytes);
    return rc;
}

/*
 * 함수 목적: 하드웨어 인터럽트(HimaxDSP 데이터 레디) 처리 메인 함수
 * 기능: UIO 공유 메모리에 2KB 단위로 SPI 수신 데이터를 덤프하고, AP(Vendor Daemon)로 플래그(0xC0)를 전달.
 * 입력: instance (센서 인스턴스), timestamp (인터럽트 발생 시간)
 * 출력: 없음
 * 리턴: 없음
 */
void himaxdsp_handle_interrupt_event(sns_sensor_instance *const instance, sns_time timestamp)
{
    himaxdsp_instance_state *state = (himaxdsp_instance_state*)instance->state->state;
    UNUSED_VAR(timestamp);

    SNS_INST_PRINTF(LOW, instance, "HimaxDSP Interrupt Received");

    uint8_t buffer[2048]; // 라인 설명: 2KB 청크 단위 SPI 수신 버퍼
    uint32_t xfer_bytes = 0;
    uint32_t total_read_bytes = 0;

    // 부팅 초기화 시점의 커널 패닉을 피하기 위해 인터럽트를 지연시킴
    if(int_count < 30) int_count = int_count + 1;
    if(int_count > 20 && qurt_init <= 3)
    {
        if(QURT_EOK == qurt_mem_pool_attach("smem_pool", &hwio_pool))
        {
            qurt_mem_region_attr_init(&hwio_attr);
            qurt_mem_region_attr_set_cache_mode(&hwio_attr, QURT_MEM_CACHE_NONE_SHARED);
            qurt_mem_region_attr_set_mapping(&hwio_attr, QURT_MEM_MAPPING_PHYS_CONTIGUOUS);
            qurt_mem_region_attr_set_physaddr(&hwio_attr, SHARED_PHYS_ADDR); // 0x81EC0000
            qurt_init = 1;

            if (QURT_EOK != qurt_mem_region_create(&shared_mem_region, SHARED_SIZE, hwio_pool, &hwio_attr)) {
                qurt_init = 2;
            } else {
                if (QURT_EOK != qurt_mem_region_attr_get(shared_mem_region, &hwio_attr)) {
                    qurt_init = 3;
                } else {
                    qurt_mem_region_attr_get_virtaddr(&hwio_attr, &myddr_base_addr); 
                    qurt_init = 4;
                }
            }
        }
    }

    if(qurt_init <= 3) {
        SNS_INST_PRINTF(HIGH, instance, "UIO Shared Mem Init In Progress... qurt_init: %d", qurt_init);
    }
    
    {  
        // 대용량 SPI 통신을 위해 Island(저전력) 모드 일시 탈출
        sns_island_exit_internal();
        
        // change(fix)-hyungchul-20260309-1400 시작
        // 설명: 동료 코드와 동일하게 헤더 길이를 11로 지정합니다.
        uint32_t header_len = HIMAXDSP_HEADER_SIZE; // 11 Bytes
        uint32_t jpeg_size = 0;
        uint32_t payload_read_bytes = 0;
        uint8_t *shared_mem_ptr = (uint8_t *)myddr_base_addr;

        // 1. SPI 통신 FIFO가 꼬이지 않도록 첫 번째부터 버퍼 최대 크기(2KB)만큼 한 번에 읽음
        uint32_t first_chunk = sizeof(buffer); 
        himaxdsp_com_read_wrapper(state->scp_service, state->com_port_info.port_handle,
                                  SNS_REG_ADDR_NONE, buffer, first_chunk, &xfer_bytes);
        total_read_bytes += xfer_bytes;

        // 2. 동료 코드와 동일한 11바이트 프로토콜 검증 (SYNC: 0xC0, 0x5A, TYPE: 0x01)
        if(buffer[0] == 0xC0 && buffer[1] == 0x5A && buffer[2] == 0x01)
        {
            // 리틀 엔디안 방식으로 11바이트 헤더의 [3~6] 위치에서 JPEG Size 추출
            jpeg_size = (buffer[6] << 24) | (buffer[5] << 16) | (buffer[4] << 8) | buffer[3];
            SNS_INST_PRINTF(HIGH, instance, "HimaxDSP Header OK. JPEG Size: %u bytes", jpeg_size);

            // UIO 메모리 크기 오버플로우 방지
            if((header_len + jpeg_size + 1) > SHARED_SIZE) 
            {
                SNS_INST_PRINTF(ERROR, instance, "Size Exceeds UIO! Truncating...");
                jpeg_size = SHARED_SIZE - header_len - 1; 
            }

            // [메모리 레이아웃]
            // [0]: Mutex Flag (0x00: ADSP 쓰는중, 0xC0: 완료됨)
            // [1~11]: Himax 11-Byte Header 원본
            // [12~N]: JPEG Payload 원본
            if(qurt_init == 4 && shared_mem_ptr != NULL)
            {
                shared_mem_ptr[0] = 0x00; // 라인 설명: 안드로이드 AP 접근 차단 플래그 (Flag Down)
                memcpy(&shared_mem_ptr[1], buffer, xfer_bytes);
            }

            if (xfer_bytes > header_len) {
                payload_read_bytes += (xfer_bytes - header_len);
            }

            // 3. JPEG 전체를 다 가져올 때까지 2KB씩 무한 청크 리딩 루프
            while(payload_read_bytes < jpeg_size)
            {
                uint32_t remain = jpeg_size - payload_read_bytes;
                uint32_t chunk = remain > sizeof(buffer) ? sizeof(buffer) : remain;
                
                himaxdsp_com_read_wrapper(state->scp_service, state->com_port_info.port_handle,
                                          SNS_REG_ADDR_NONE, buffer, chunk, &xfer_bytes);

                if(qurt_init == 4 && shared_mem_ptr != NULL)
                {
                    // 기록할 위치 = 시작주소 + 플래그(1) + 헤더(11) + 지금까지 쓴 크기
                    void* current_dest = (void*)(shared_mem_ptr + 1 + header_len + payload_read_bytes);
                    memcpy(current_dest, buffer, xfer_bytes);
                }
                
                payload_read_bytes += xfer_bytes;
                total_read_bytes += xfer_bytes;
            }

            qurt_mem_barrier(); 

            // 4. 쓰기 완료. 안드로이드 AP에 읽어가라고 플래그 세팅
            if(qurt_init == 4 && shared_mem_ptr != NULL)
            {
                shared_mem_ptr[0] = 0xC0; // 라인 설명: AP 접근 허용 플래그 (Flag Up)
            }
            
            state->current_frame_id++; 
        }
        else
        {
            SNS_INST_PRINTF(ERROR, instance, "Header Sync Failed! Found: [%02X %02X %02X]", buffer[0], buffer[1], buffer[2]);
        }
        // change(fix)-hyungchul-20260309-1400 끝
    }

    SNS_INST_PRINTF(HIGH, instance, "Total read bytes: %u", total_read_bytes);
}

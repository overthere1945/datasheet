/*
 * 목적: HimaxDSP 드라이버 설정을 위한 상수 및 매크로 정의
 * 기능: SPI, GPIO 핀 번호 및 통신 프로토콜 사이즈 정의
 * change(fix)-hyungchul-20260309-1400: 헤더 사이즈를 동료 코드와 동일하게 11바이트로 수정
 */

#pragma once

#include "sns_interface_defs.h"

/* ==========================================================================
   하드웨어 설정 (SPI & GPIO)
   ========================================================================== */

/* SPI Bus Instance: SE4 -> 5 (SSC_QUP_SE4) */
#define HIMAXDSP_BUS_INSTANCE        5

/* Bus Type: SPI */
#define HIMAXDSP_BUS_TYPE            SNS_BUS_SPI

/* SPI Slave Control: Chip Select Index 0 */
#define HIMAXDSP_SLAVE_CONTROL       0

/* SPI Clock Speed: Min 10MHz, Max 12MHz */
#define HIMAXDSP_BUS_MIN_FREQ_KHZ    10000
#define HIMAXDSP_BUS_MAX_FREQ_KHZ    12000

/* Interrupt GPIO Pin Number: GPIO_102 */
#define HIMAXDSP_IRQ_NUM             102

/* Wake-Up GPIO Pin Number: GPIO_105 */
#define HIMAXDSP_WAKEUP_NUM          105

// ADSP 메모리 부족 방지를 위해 청크 사이즈를 2KB로 정의
#define HIMAXDSP_MAX_CHUNK_SIZE      2048

/* ==========================================================================
   레지스트리 설정
   ========================================================================== */
#ifndef HIMAXDSP_CONFIG_ENABLE_SEE_LITE
#define HIMAXDSP_CONFIG_ENABLE_SEE_LITE   0
#endif

#define HIMAXDSP_DISABLE_REGISTRY    0

/* ==========================================================================
   HimaxDSP 프로토콜 정의
   ========================================================================== */
/* change(fix)-hyungchul-20260309-1400 시작
 * 설명: 동료 코드의 프로토콜 분석 결과에 따라 헤더를 11바이트로 수정합니다.
 * [0]: SYNC0 (0xC0)
 * [1]: SYNC1 (0x5A)
 * [2]: TYPE (0x01)
 * [3-6]: JPEG Size (Little Endian)
 * [7]: Low Illumination
 * [8]: Similarity
 * [9]: Scene
 * [10]: Occlusion Probability
 */
#define HIMAXDSP_HEADER_SIZE         11
// change(fix)-hyungchul-20260309-1400 끝

#ifndef SNS_REG_ADDR_NONE
#define SNS_REG_ADDR_NONE 0
#endif

/* 초기화 기본값 매크로 */
#define himaxdsp_com_port_cfg_init_def {             \
    .bus_type          = HIMAXDSP_BUS_TYPE,          \
    .slave_control     = HIMAXDSP_SLAVE_CONTROL,     \
    .reg_addr_type     = SNS_REG_ADDR_8_BIT,         \
    .min_bus_speed_KHz = HIMAXDSP_BUS_MIN_FREQ_KHZ,  \
    .max_bus_speed_KHz = HIMAXDSP_BUS_MAX_FREQ_KHZ,  \
    .bus_instance      = HIMAXDSP_BUS_INSTANCE       \
}

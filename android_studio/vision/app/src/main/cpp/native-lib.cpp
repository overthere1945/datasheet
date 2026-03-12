/*
 * 파일 목적: Android Application과 커널 공간(UIO 공유 메모리)을 연결하는 JNI 브릿지 인터페이스
 * 파일 기능:
 * 1. Java 측에서 센서 인터럽트를 수신했을 때(1FPS 저빈도) 호출되어 메모리 읽기 수행
 * 2. 128KB(0x20000) 크기의 /dev/uio1 디바이스 노드를 mmap으로 매핑
 * 3. (해결) Early Interrupt로 인해 Java가 너무 일찍 깼을 경우, 최대 300ms까지 0xC0 플래그를 대기
 * 4. 무한 루프(Polling) 없이 단발성으로 작동하여 시스템 리소스를 절약하고 디코딩용 byte[] 반환
 */

#include <jni.h>
#include <string>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>
#include <android/log.h>
#include <atomic>

#define TAG "VISION_NATIVE"

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_example_vision_MainActivity_readFromUio(JNIEnv *env, jobject thiz, jboolean filtered) {

    const size_t UIO_SIZE = 128 * 1024;

    int fd = open("/dev/uio1", O_RDWR | O_SYNC);
    if (fd < 0) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to open /dev/uio1");
        return nullptr;
    }
    __android_log_print(ANDROID_LOG_INFO, TAG, "open /dev/uio1");

    void* ptr = mmap(NULL, UIO_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to mmap");
        close(fd);
        return nullptr;
    }
    __android_log_print(ANDROID_LOG_INFO, TAG, "mmap success");

    volatile uint8_t* byte_ptr = (volatile uint8_t*)ptr;
    jbyteArray result = nullptr;

    // change(fix)-hyungchul-20260311-1056 시작
    // 설명: ADSP가 이미지 복사를 100% 완료한 후 인터럽트를 쏘도록 구조를 되돌렸으므로,
    // 300ms 대기(스레드 블로킹) 로직을 완전히 삭제하고 즉시 읽도록 원복합니다.
    if (byte_ptr[0] == 0xC0) {

        std::atomic_thread_fence(std::memory_order_acquire);

        uint8_t value_1byte[12];
        for (size_t i = 0; i < 12; i++) {
            value_1byte[i] = byte_ptr[i];
        }

        uint32_t jpgsize = (uint32_t)value_1byte[4] |
                           ((uint32_t)value_1byte[5] << 8) |
                           ((uint32_t)value_1byte[6] << 16) |
                           ((uint32_t)value_1byte[7] << 24);

        if (jpgsize > 0 && jpgsize < (UIO_SIZE - 12)) {
            bool drop = false;
            if (filtered && (value_1byte[9] >= 80)) {
                __android_log_print(ANDROID_LOG_DEBUG, TAG, "==> Too similar filtered");
                drop = true;
            }

            if (!drop) {
                result = env->NewByteArray(jpgsize);
                if (result != nullptr) {
                    env->SetByteArrayRegion(result, 0, jpgsize, (jbyte*)(byte_ptr + 12));
                    __android_log_print(ANDROID_LOG_DEBUG, TAG, "JPEG Size: %u bytes read success.", jpgsize);
                }
            }
        }

        std::atomic_thread_fence(std::memory_order_release);
        byte_ptr[0] = 0x00;
    } else {
        // 수동 터치로 읽었을 때 아직 플래그가 없으면 경고 출력 (사용자 수정 반영)
        if (!filtered) {
            __android_log_print(ANDROID_LOG_WARN, TAG, "UIO memory flag is not 0xC0. ADSP data not ready.");
        }
    }
    // change(fix)-hyungchul-20260311-1056 끝

    munmap(ptr, UIO_SIZE);
    close(fd);

    return result;
}
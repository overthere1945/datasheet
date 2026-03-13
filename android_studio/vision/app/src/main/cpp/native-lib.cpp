/*
 * 목적: Android Application의 JNI(Java Native Interface) 브릿지 역할을 하며,
 * Native 백그라운드 스레드를 통해 UIO 공유 메모리를 지속적으로 모니터링하여 JPEG 이미지를 읽어옴.
 * 기능:
 * 1. JNI OnLoad 시 JavaVM 포인터를 획득하여 C++ 스레드에서 Java 콜백이 가능하게 설정
 * 2. 128KB(0x20000) 크기의 메모리를 매핑하고, 10ms 주기로 첫 번째 바이트(플래그)를 검사
 * 3. ADSP 플래그 0xC0 감지 시, 파일 I/O 충돌을 막기 위해 JPEG 바이트 배열을 Java로 직접 전송함
 */

#include <jni.h>      // JNI 연동을 위한 헤더 포함
#include <string>     // C++ 문자열 처리를 위한 헤더 포함
#include <fcntl.h>    // O_RDWR, O_SYNC 매크로 정의 포함 (파일 제어)
#include <sys/mman.h> // mmap 메모리 맵핑 함수 정의 포함
#include <unistd.h>   // close 정의 포함
#include <stdint.h>   // uint32_t 정의 포함
#include <sys/stat.h> // open 관련 상태 및 매크로 정의 포함
#include <android/log.h> // 안드로이드 Logcat 출력을 위한 헤더 포함
#include <thread>     // C++ 백그라운드 스레드 사용
#include <atomic>     // 스레드 안전성 변수 사용
#include <vector> // change(add)-hyungchul-20260306-1720: 안전한 메모리 복사를 위해 vector 추가

// Logcat에 출력될 기본 태그명 정의
#define TAG "VISION_NATIVE"

// change(add)-hyungchul-20260306-1610 시작
// 설명: 센서 프레임워크 제약을 우회하기 위해 C++ 네이티브 스레드 방식을 도입합니다.
JavaVM* g_jvm = nullptr;                  // 스레드에서 JNIEnv를 얻기 위한 전역 JVM 포인터
jobject g_main_activity = nullptr;        // Java 측 MainActivity 인스턴스 (글로벌 참조)
jmethodID g_on_image_received = nullptr;  // Java 측 콜백 메서드 ID
std::atomic<bool> g_is_running(false);    // 폴링 스레드 실행 여부 제어
std::thread g_polling_thread;             // 실제 동작할 백그라운드 스레드

/*
 * 목적: 공유 라이브러리가 로드될 때 안드로이드 시스템에 의해 가장 먼저 호출됨
 * 기능: 전역 JavaVM 포인터를 획득하여 추후 다른 스레드에서 JNIEnv를 붙일 수 있게 함
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_jvm = vm;
    return JNI_VERSION_1_6;
}

/*
 * 목적: 무한 루프를 돌며 UIO 메모리의 상태 플래그를 모니터링하는 스레드 메인 함수
 * 기능: 10ms 단위로 mmap 영역을 검사하며, 새 프레임이 오면 파일로 저장하고 Java UI를 호출함
 */
void pollingThreadFunc(bool filtered) {
    JNIEnv *env;
    // 백그라운드 스레드를 Java VM에 부착(Attach)하여 JNI 함수 호출 권한을 획득
    int envStat = g_jvm->GetEnv((void **)&env, JNI_VERSION_1_6);
    if (envStat == JNI_EDETACHED) {
        if (g_jvm->AttachCurrentThread(&env, NULL) != 0) {
            __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to attach thread");
            return;
        }
    }
    // 설명: ADSP 드라이버 및 커널 DTSI에서 128KB(0x20000)로 공유 메모리를 확장하였으므로 매핑 크기를 동기화합니다.
    const size_t UIO_SIZE = 128 * 1024; // 128KB (uint8기준) 매핑 사이즈 설정
    // change(add)-hyungchul-20260306-1357 끝
	
    // 2. 장치 열기 (O_SYNC로 캐싱 방지하여 메모리 일관성 확보)
    int fd = open("/dev/uio1", O_RDWR  | O_SYNC);
    // 파일 디스크립터가 음수이면 열기 실패이므로 false 반환
    if (fd < 0) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to open /dev/uio1");
        g_jvm->DetachCurrentThread();
        return;
    }

    // 3. 메모리 매핑 (장치 파일의 물리 메모리를 사용자 공간으로 매핑)
    void* ptr = mmap(NULL, UIO_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    // 매핑에 실패한 경우 장치를 닫고 false 반환
    if (ptr == MAP_FAILED) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to mmap");
        close(fd);
        g_jvm->DetachCurrentThread();
        return;
    }

    //[00] 5a -> Sync
    //[01] 01 -> JPEG
    //[02-05] 3c360000 -> JPEG size (13884byte)
    //[06] 05 -> 저조도 값
    //[07] 59 -> 유사도 값
    //[08] 04 -> sence 값
    //[09] 0c -> occlusion 값
    //[10] FFD8 -> JPEG SOI(Start Of Image)
    //FFD9 -> JPEG EOI(End Of Image)

    // 4. Header확인.
    // 매핑된 메모리를 1바이트(uint8_t) 단위로 접근하기 위해 포인터 캐스팅 (최적화 방지를 위해 volatile 사용)
    volatile uint8_t* byte_ptr = (volatile uint8_t*)ptr;
    __android_log_print(ANDROID_LOG_INFO, TAG, "Native Polling Thread Started.");

    // 스레드가 종료 명령을 받을 때까지 무한 반복
    while (g_is_running) {
        // ADSP가 모든 데이터 작성을 완료하고 남긴 0xC0(플래그 업)을 감지!
        if (byte_ptr[0] == 0xC0) {
            // change(add)-hyungchul-20260306-1745 시작
            // 설명: ARM64 CPU는 플래그(0xC0)가 바뀌기도 전에 예측 실행으로 데이터 캐시를 오염시킬 수 있습니다.
            // acquire 메모리 배리어를 걸어서, 반드시 0xC0 플래그 검사가 끝난 "이후"에만 뒤쪽 데이터를 읽도록 강제합니다.
            std::atomic_thread_fence(std::memory_order_acquire);
            
            // 1. 헤더 파싱 및 크기 추출
            uint8_t value_1byte[12];
    // 헤더 길이만큼 반복문 수행
    for (size_t i = 0; i <12; i++) {
        // 공유 메모리에서 1바이트씩 헤더 배열로 복사
        value_1byte[i] = byte_ptr[i];
    }
    
    // 리틀 엔디안 방식으로 4바이트 JPEG 크기 데이터를 파싱하여 조합 (인덱스 3~6)
    uint32_t jpgsize = (uint32_t)value_1byte[3] |
                      ((uint32_t)value_1byte[4] << 8) |
                      ((uint32_t)value_1byte[5] << 16) |
                      ((uint32_t)value_1byte[6] << 24);

            // 비정상적인 데이터 크기는 드롭합니다. (최소 4바이트 보장)
            if (jpgsize >= 0x20000 || jpgsize < 4) {
                __android_log_print(ANDROID_LOG_DEBUG, TAG, "==> Invalid JPG Size (0x%04x)", jpgsize);
                byte_ptr[0] = 0x00; // 오류 시에도 다음 프레임을 위해 플래그 초기화
                continue;
            }
            
    // 6.  유사도 판별 (80%이상 동일시 return)
    // filtered 플래그가 켜져있고, 인덱스 8의 값이 80 이상인지 확인 (유사도 검사)
            if (filtered && (value_1byte[8] >= 80)) {
        // 이전 이미지와 너무 유사하여 필터링 되었음을 로그로 출력
        __android_log_print(ANDROID_LOG_DEBUG, TAG,"==> Too similar filtered (%d)", value_1byte[7]);
                byte_ptr[0] = 0x00; 
                continue;
    }

            // change(add)-hyungchul-20260306-1720 시작
            // 설명: 기존에 4바이트 단위로 패딩이 들어가던 문제를 수정하여 정확히 jpgsize 만큼만 안전하게 복사합니다.
            std::vector<uint8_t> temp_buf(jpgsize);
            memcpy(temp_buf.data(), (const void *) (byte_ptr + 11), jpgsize);

            // JPEG 무결성 확인용 디버그 로그: 정상적인 JPEG 파일은 항상 FF D8로 시작해서 FF D9로 끝납니다.
            __android_log_print(ANDROID_LOG_DEBUG, TAG, "JPEG Header check: [%02x %02x %02x %02x], Tail: [%02x %02x]", 
                                temp_buf[0], temp_buf[1], temp_buf[2], temp_buf[3],
                                temp_buf[jpgsize-2], temp_buf[jpgsize-1]);

    // 안드로이드 공용 다운로드 폴더 내의 저장 파일 경로 지정
    const char* filePath = "/mnt/sdcard/Download/vga.jpg";
    // 지정된 경로에 바이너리 쓰기 모드("wb")로 파일 열기
    FILE* fp = fopen(filePath, "wb"); // wb: 바이너리 쓰기 모드

    // 파일이 정상적으로 열렸는지 확인
    if (fp != NULL) {
                // 정확한 바이트 수(1바이트 단위 * jpgsize)로 기록하여 파일 훼손을 방지합니다.
                fwrite(temp_buf.data(), 1, jpgsize, fp);

        // 3. 파일 닫기 및 동기화 (버퍼 내용을 디스크에 강제 쓰기)
        fflush(fp);
        // 열어둔 파일 스트림 닫기
        fclose(fp);
                __android_log_print(ANDROID_LOG_DEBUG, TAG, "Image saved. Size: %u bytes", jpgsize);
            }

            // 4. Java 측 화면 업데이트 콜백 호출 (파일 I/O 레이스 컨디션 방지를 위해 바이트 배열을 직접 쏩니다)
            if (g_main_activity != nullptr && g_on_image_received != nullptr) {
                // JNI 바이트 배열 생성 및 데이터 복사
                jbyteArray jBuf = env->NewByteArray(jpgsize);
                if (jBuf != nullptr) {
                    env->SetByteArrayRegion(jBuf, 0, jpgsize, (jbyte*)temp_buf.data());
                    // Java의 onImageReceived(byte[]) 함수를 호출하며 데이터 전달
                    env->CallVoidMethod(g_main_activity, g_on_image_received, jBuf);
                    // 중요: 메모리 릭(Leak)을 방지하기 위해 로컬 참조 반드시 해제
                    env->DeleteLocalRef(jBuf);
                }
            }

            // 다음 프레임을 받을 수 있도록 완료 처리 (release 배리어로 메모리 쓰기 순서 강제)
            std::atomic_thread_fence(std::memory_order_release);
            byte_ptr[0] = 0x00; 
            // change(add)-hyungchul-20260306-1745 끝
        }

        // CPU 100% 점유율 방지를 위해 10ms(10000 microseconds) 대기. 배터리 소모 최소화.
        usleep(10000); 
    }

    // 종료 시 리소스 해제
    munmap(ptr, UIO_SIZE);
    // UIO 장치 파일 닫기
    close(fd);

    if (envStat == JNI_EDETACHED) {
        g_jvm->DetachCurrentThread();
    }
    __android_log_print(ANDROID_LOG_INFO, TAG, "Native Polling Thread Stopped.");
}

/*
 * 목적: Java 영역에서 스레드 시작을 요청할 때 호출되는 함수
 */
extern "C" JNIEXPORT void JNICALL
Java_com_example_vision_MainActivity_startNativeThread(JNIEnv *env, jobject thiz, jboolean filtered) {
    if (g_is_running) return; // 이미 실행 중이면 무시
    
    // Java 객체와 콜백 메서드(onImageReceived)의 ID를 미리 캐싱하여 스레드에서 사용 가능하게 함
    g_main_activity = env->NewGlobalRef(thiz);
    jclass clazz = env->GetObjectClass(g_main_activity);
    // change(add)-hyungchul-20260306-1720 시작
    // 설명: 콜백 함수의 시그니처를 인자 없는 "()V" 에서 byte[] 배열을 받는 "([B)V" 로 변경합니다.
    g_on_image_received = env->GetMethodID(clazz, "onImageReceived", "([B)V");
    // change(add)-hyungchul-20260306-1720 끝
    
    g_is_running = true;
    g_polling_thread = std::thread(pollingThreadFunc, filtered);
}

/*
 * 목적: Java 영역에서 스레드 중지를 요청할 때 호출되는 함수
 */
extern "C" JNIEXPORT void JNICALL
Java_com_example_vision_MainActivity_stopNativeThread(JNIEnv *env, jobject thiz) {
    if (g_is_running) {
        g_is_running = false; // 스레드의 루프 종료 지시
        if (g_polling_thread.joinable()) {
            g_polling_thread.join(); // 스레드가 안전하게 종료될 때까지 대기
        }
        env->DeleteGlobalRef(g_main_activity);
        g_main_activity = nullptr;
    }
}
// change(add)-hyungchul-20260306-1610 끝

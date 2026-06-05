/** 공통 매크로, 에러 코드, 네트워크 데이터 구조체 */
#ifndef COMMON_H
#define COMMON_H

// 기본 설정
#define MAX_LIB_NAME 64
#define MAX_SYMBOL 64
#define MAX_BASE_PATH 256
#define MAX_PATH 512
#define MAX_BUF 1024

#define MAX_PWM 100
#define INIT_MIN_PWM 0

#define LED_DEFAULT_VAL 70

#define TCP_BACKLOG_LEN 5

// 디렉토리 초기화 및 경로 반환 함수
void init_base_path();
const char* get_base_path();
#define MAKE_ABSOLUTE_PATH(dest_buf, size, relative_path) \
        snprintf(dest_buf, size, "%s/%s", get_base_path(), relative_path);

// 로그 매크로
#include <syslog.h>
#define SYSTEM_LOG(fmt,...) { \
        openlog("Veda Linux Project Log", LOG_CONS | LOG_PID, LOG_USER); \
        syslog(LOG_INFO, fmt, ##__VA_ARGS__); \
        closelog(); \
}

// 장치 관리 함수
int init_device_manager();
void cleanup_devices();

// 서버 제어 함수
int start_network_server();

// 시그널 제어 플래그
#include <signal.h>
extern volatile sig_atomic_t g_reload_req; // 핫스왑 시그널
extern volatile sig_atomic_t g_running;

#endif
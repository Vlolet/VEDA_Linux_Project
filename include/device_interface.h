/** 동적 라이브러리용 공통 인터페이스 구조체 */
#ifndef DEVICE_INTERFACE_H
#define DEVICE_INTERFACE_H

/** 장치 종류 리스트 */
typedef enum {
    TYPE_LED,
    TYPE_BUZZER,
    TYPE_CDS,
    TYPE_SEGMENT,
} DeviceType;

/** 장치 제어 명령 리스트 */
typedef enum {
    CMD_LED_ON              = 101,
    CMD_LED_OFF             = 102,
    CMD_LED_SET_VALUE       = 103,

    CMD_BUZZER_ON           = 201,
    CMD_BUZZER_OFF          = 202,

    CMD_CDS_AUTO_ON         = 301,
    CMD_CDS_AUTO_OFF        = 302,
    CMD_CDS_SET_THRESHOLD   = 303,
    CMD_SENSOR_DATA         = 304,

    CMD_SEGMENT_SET_DIGIT   = 401,
    CMD_SEGMENT_CLEAR       = 402,
    CMD_SEGMENT_SET_RAW     = 403,
    CMD_SEGMENT_COUNTDOWN = 404,
    CMD_SEGMENT_TEST_RAW    = 405
} DeviceCmd;

/** 모든 장치 동적 라이브러리가 포함해야 함 */
typedef struct {
    int (*init)(void* params);
    int (*control)(int cmd, int val);
    int (*read)(void* buf);
    void (*exit)(void);
} DeviceInterface;

/** 커맨드와 값을 담는 구조체(스레드에서 사용) */
typedef struct {
    DeviceCmd cmd;
    int val;
} ControlPacket;

/** device_manager.c에서 정의한 공용 함수 */
DeviceInterface* lock_and_get_device(DeviceType type);
void release_device(DeviceType type);
void reload_devices();

extern int g_client_fd;

/** 명령 handler(인터페이스) 함수 */
int led_handler_control(int cmd, int val);
int buzzer_handler_control(int cmd);
int cds_handler_control(int cmd, int val);
int segment_handler_control(int cmd, int val);

#endif
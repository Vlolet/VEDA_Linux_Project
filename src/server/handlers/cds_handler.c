#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

#include "common.h"
#include "device_interface.h"

int cds_handler_control(int cmd, int val);
static void* cds_monitor_thread(void *arg);
static void send_sensor_data_to_client(int val);

static pthread_t cds_thread;
static bool cds_thread_running = false;
static int threshold = 180;

int cds_handler_control(int cmd, int val){
    if (cmd == CMD_CDS_AUTO_ON) {
        if (cds_thread_running) {
            SYSTEM_LOG("[cds_handler] CDS Monitor thread is already running.");
            return 0;
        }
        cds_thread_running = true;
        if (pthread_create(&cds_thread, NULL, cds_monitor_thread, NULL) != 0) {
            SYSTEM_LOG("[cds_handler] failed to create CDS thread");
            cds_thread_running = false;
            return -1;
        }
        SYSTEM_LOG("[cds_handler] CDS automatic mode started.");
    } 
    else if (cmd == CMD_CDS_AUTO_OFF) {
        if (cds_thread_running) {
            cds_thread_running = false;
            pthread_join(cds_thread, NULL);
            SYSTEM_LOG("[cds_handler] CDS automatic mode stopped.");
        }
    } 
    else if (cmd == CMD_CDS_SET_THRESHOLD) {
        threshold = val;
        SYSTEM_LOG("[cds_handler] CDS threshold updated to %d.", threshold);
    }
    return 0;
}

// 실시간 조도 센서 모니터링 및 자동 제어 루프 스레드
static void* cds_monitor_thread(void *arg) {
    SYSTEM_LOG("[cds_handler] CDS monitoring thread started.");
    int prev_val = -1;

    while (cds_thread_running) {
        int current_val = -1;

        // 조도 센서 데이터 
        DeviceInterface *cds_ops = lock_and_get_device(TYPE_CDS);
        if (cds_ops && cds_ops->read) {
            cds_ops->read(&current_val);
        }
        release_device(TYPE_CDS);

        if (current_val != -1) {
            // 측정값 >= threshold(어두움) 최초 진입할 때 LED ON
            if (current_val >= threshold && (prev_val < threshold || prev_val == -1)) {
                SYSTEM_LOG("[cds_handler] Dark detected (%d >= %d). Trigger LED ON.", current_val, threshold);
                led_handler_control(CMD_LED_ON, 0);
            } 
            // 측정값 < threshold(밝아짐) 최초 진입할 때 LED OFF
            else if (current_val < threshold && (prev_val >= threshold || prev_val == -1)) {
                SYSTEM_LOG("[cds_handler] Bright detected (%d < %d). Trigger LED OFF.", current_val, threshold);
                led_handler_control(CMD_LED_OFF, 0);
            }

            prev_val = current_val;

            send_sensor_data_to_client(current_val);
        }

        // 0.5초 단위 모니터링
        usleep(500000);
    }

    SYSTEM_LOG("[cds_handler] CDS monitoring thread exiting.");
    return NULL;
}

// 조도 센서 값을 TCP 소켓으로 송신
static void send_sensor_data_to_client(int val) {
    if (g_client_fd > 0) {
        ControlPacket pkt;
        pkt.cmd = CMD_SENSOR_DATA;
        pkt.val = val;
        
        if (write(g_client_fd, &pkt, sizeof(ControlPacket)) < 0) {
            SYSTEM_LOG("[cds_handler] write to client failed. socket descriptor may be closed.");
        }
    }
}
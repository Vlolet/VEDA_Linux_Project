#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>

#include "common.h"
#include "device_interface.h"

static pthread_t segment_thread;
static volatile bool segment_thread_running = false;

static int g_countdown_start_val = 9;

static void send_countdown_to_client(int digit) {
    if (g_client_fd > 0) {
        ControlPacket pkt;
        // 클라이언트에게 이 패킷이 카운트다운 정보임을 알리기 위해 CMD_SEGMENT_COUNTDOWN 사용
        pkt.cmd = CMD_SEGMENT_COUNTDOWN; 
        pkt.val = digit;
        
        if (write(g_client_fd, &pkt, sizeof(ControlPacket)) < 0) {
            SYSTEM_LOG("[segment_handler] write countdown to client failed. socket closed?");
        }
    }
}

// 9 ~ 1 카운트다운 백그라운드 스레드
static void* segment_countdown_worker(void *arg) {
    int start_digit = g_countdown_start_val;
    SYSTEM_LOG("[segment_handler] Countdown test thread started from: %d", start_digit);
    
    for (int digit = start_digit; digit >= 0 && segment_thread_running; digit--) {
        DeviceInterface *ops = lock_and_get_device(TYPE_SEGMENT);
        if (ops && ops->control) {
            ops->control(CMD_SEGMENT_SET_DIGIT, digit);
        }
        release_device(TYPE_SEGMENT);

        send_countdown_to_client(digit);

        if(digit == 0) buzzer_handler_control(CMD_BUZZER_ON);

        sleep(1); 
    }

    DeviceInterface *ops = lock_and_get_device(TYPE_SEGMENT);
    if (ops && ops->control) {
        ops->control(CMD_SEGMENT_CLEAR, 0);
    }
    release_device(TYPE_SEGMENT);

    segment_thread_running = false;
    SYSTEM_LOG("[segment_handler] Countdown test thread completed.");
    return NULL;
}

// a ~ g 세그먼트 개별 순차 테스트 스레드
static void* segment_raw_worker(void *arg) {
    SYSTEM_LOG("[segment_handler] Raw segment test thread started.");

    for (char seg = 'a'; seg <= 'h' && segment_thread_running; seg++) {
        DeviceInterface *ops = lock_and_get_device(TYPE_SEGMENT);
        if (ops && ops->control) {
            ops->control(CMD_SEGMENT_SET_RAW, seg);
        }
        release_device(TYPE_SEGMENT);

        sleep(1);
    }

    DeviceInterface *ops = lock_and_get_device(TYPE_SEGMENT);
    if (ops && ops->control) {
        ops->control(CMD_SEGMENT_CLEAR, 0);
    }
    release_device(TYPE_SEGMENT);

    segment_thread_running = false;
    SYSTEM_LOG("[segment_handler] Raw segment test thread completed.");
    return NULL;
}

static void stop_current_test() {
    if (segment_thread_running) {
        segment_thread_running = false;
        pthread_join(segment_thread, NULL);
    }
}

int segment_handler_control(int cmd, int val) {
    if (cmd == CMD_SEGMENT_SET_DIGIT || cmd == CMD_SEGMENT_SET_RAW || cmd == CMD_SEGMENT_CLEAR) {
        stop_current_test();
        
        DeviceInterface *ops = lock_and_get_device(TYPE_SEGMENT);
        if (ops && ops->control) {
            ops->control(cmd, val);
        }
        release_device(TYPE_SEGMENT);
        return 0;
    }

    if (cmd == CMD_SEGMENT_COUNTDOWN) {
        stop_current_test();
        if(val<1 || val>9) val = 9;
        g_countdown_start_val = val;
        segment_thread_running = true;
        if (pthread_create(&segment_thread, NULL, segment_countdown_worker, NULL) != 0) {
            SYSTEM_LOG("[segment_handler] failed to create countdown thread");
            segment_thread_running = false;
            return -1;
        }
        pthread_detach(segment_thread);
    }

    else if (cmd == CMD_SEGMENT_TEST_RAW) {
        stop_current_test();
        segment_thread_running = true;
        if (pthread_create(&segment_thread, NULL, segment_raw_worker, NULL) != 0) {
            SYSTEM_LOG("[segment_handler] failed to create raw test thread");
            segment_thread_running = false;
            return -1;
        }
        pthread_detach(segment_thread);
    }

    return 0;
}
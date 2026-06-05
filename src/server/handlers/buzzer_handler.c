// src/server/handlers/buzzer_handler.c

#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

#include "common.h"
#include "device_interface.h"

typedef struct {
    int cmd;
} BuzzerThreadArgs;

// 부저 동작 워커 스레드 함수
static void* buzzer_worker_thread(void *arg) {
    BuzzerThreadArgs *args = (BuzzerThreadArgs*)arg;
    int cmd = args->cmd;
    free(args); 

    DeviceInterface *ops = lock_and_get_device(TYPE_BUZZER);
    if (!(ops && ops->control)) {
        SYSTEM_LOG("[buzzer_handler] no Buzzer device operations available");
        release_device(TYPE_BUZZER);
        return NULL;
    }

    if (cmd == CMD_BUZZER_ON) {
        ops->control(CMD_BUZZER_ON, 0);
        release_device(TYPE_BUZZER);

        ops = lock_and_get_device(TYPE_BUZZER);
        if (ops && ops->control) {
            ops->control(CMD_BUZZER_OFF, 0);
        }
        release_device(TYPE_BUZZER);
    } else {
        ops->control(cmd, 0);
        release_device(TYPE_BUZZER);
    }

    return NULL;
}

int buzzer_handler_control(int cmd) {
    pthread_t thread_id;
    BuzzerThreadArgs *args = (BuzzerThreadArgs*)malloc(sizeof(BuzzerThreadArgs));
    if (!args) {
        SYSTEM_LOG("[buzzer_handler] memory allocation failed");
        return -1;
    }
    args->cmd = cmd;

    if (pthread_create(&thread_id, NULL, buzzer_worker_thread, args) != 0) {
        SYSTEM_LOG("[buzzer_handler] failed to create buzzer worker thread");
        free(args);
        return -1;
    }

    pthread_detach(thread_id);
    return 0;
}
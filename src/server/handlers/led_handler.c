#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>

#include "common.h"
#include "device_interface.h"

typedef struct{
    DeviceCmd cmd;
    int val;
}DeviceArgs;

static int g_led_pwm_val = 70;
static bool g_led_state_on = false;

int led_handler_control(int cmd, int val);
static void* led_worker_thread(void *arg);

static void notify_client_led_state(int state_cmd) {
    if (g_client_fd > 0) {
        ControlPacket pkt;
        pkt.cmd = state_cmd; // CMD_LED_ON 또는 CMD_LED_OFF
        pkt.val = 0;
        if (write(g_client_fd, &pkt, sizeof(ControlPacket)) < 0) {
            SYSTEM_LOG("[led_handler] Failed to notify client socket.");
        }
    }
}

int led_handler_control(int cmd, int val){
    if(cmd == CMD_LED_SET_VALUE){
        g_led_pwm_val = val;
        if (!g_led_state_on) {
            SYSTEM_LOG("[led_handler] PWM value stored: %d (LED is currently OFF. Not turned ON)", val);
            return 0; 
        }
    } else if (cmd == CMD_LED_ON){
        g_led_state_on = true;
        if(INIT_MIN_PWM <= val && val <= MAX_PWM) g_led_pwm_val = val;
    } else if (cmd == CMD_LED_OFF){
        g_led_state_on = false;
    }
    pthread_t thread_id;
    DeviceArgs *args = (DeviceArgs*)malloc(sizeof(DeviceArgs));
    if(!args){
        SYSTEM_LOG("[led_handler] memory allocation failed");
        return -1;
    }
    args->cmd = cmd;
    if (cmd == CMD_LED_ON) args->val = g_led_pwm_val;
    else args->val = val;

    if(pthread_create(&thread_id, NULL, led_worker_thread, args) != 0){
        SYSTEM_LOG("[led_handler] failed to create LED worker thread");
        free(args);
        return -1;
    }

    pthread_detach(thread_id);
    SYSTEM_LOG("[led_handler] success to create LED worker thread");
    return 0;
}

static void* led_worker_thread(void *arg) {
    DeviceArgs *args = (DeviceArgs*)arg;
    int cmd = args->cmd;
    int val = args->val;
    free(args);

    DeviceInterface *ops = lock_and_get_device(TYPE_LED);
    if (!(ops && ops->control)) {
        SYSTEM_LOG("[led_handler] no LED device operations available");
        release_device(TYPE_LED);
        return NULL;
    }

    ops->control(cmd, val);
    release_device(TYPE_LED);

    if (cmd == CMD_LED_ON || cmd == CMD_LED_OFF) {
        notify_client_led_state(cmd);
    }

    return NULL;
}
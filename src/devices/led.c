#include <stdio.h>
#include <wiringPi.h>
#include <softPwm.h>

#include "common.h"
#include "device_interface.h"

int pin;

int led_init(void* params){
    // wiringPiSetupGpio 함수는 device_manager.c로 이동함
    // wiringPiSetupGpio();
    pin = *(int*)params;
    pinMode(pin, OUTPUT);
    softPwmCreate(pin, INIT_MIN_PWM, MAX_PWM);
    SYSTEM_LOG("[LIBRARY] LED initialized on Pin %d", pin);
    return 0;
}

int led_control(int cmd, int val){
    SYSTEM_LOG("[LIBRARY] LED cmd %d, val %d", cmd, val);
    // SYSTEM_LOG("[LIBRARY-HOTSWAP] LED cmd %d, val %d", cmd, val);
    if(cmd == CMD_LED_ON)
        softPwmWrite(pin, val > INIT_MIN_PWM ? val : MAX_PWM);
    else if(cmd == CMD_LED_OFF){
        softPwmWrite(pin, 0);
        digitalWrite(pin, LOW);
    }
    else if(cmd == CMD_LED_SET_VALUE){
        softPwmWrite(pin, val);
    }
    return 0;
}

void led_exit(){
    // exit할게 있나..?
    SYSTEM_LOG("[LIBRARY] LED exit %d", pin);
    softPwmStop(pin);
    return;
}

DeviceInterface device_ops = {
    .init = led_init,
    .control = led_control,
    .read = NULL,
    .exit = led_exit
};
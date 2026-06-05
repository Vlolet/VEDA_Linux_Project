#include <stdio.h>
#include <wiringPi.h>
#include <softTone.h>

#include "common.h"
#include "device_interface.h"

int notes[] = {
    391, 391, 440, 440, 391, 391, 329, 329, 0
};

int pin;

static volatile bool g_melody_playing = false;


int buzzer_init(void* params){
    // wiringPiSetupGpio 함수는 device_manager.c로 이동함
    // wiringPiSetupGpio();
    pin = *(int*)params;
    pinMode(pin, OUTPUT);
    softToneCreate(pin);
    SYSTEM_LOG("[LIBRARY] BUZZER initialized on Pin %d", pin);
    return 0;
}

int buzzer_control(int cmd, int val){
    SYSTEM_LOG("[LIBRARY] BUZZER cmd %d, val %d", cmd, val);
    if(cmd == CMD_BUZZER_ON){
        g_melody_playing = true;
        int num_notes = sizeof(notes)/sizeof(int);
        for(int i=0; i<num_notes && g_melody_playing; i++){
            softToneWrite(pin, notes[i]);
            delay(280);
        }
        g_melody_playing = false;
    }
    else if(cmd == CMD_BUZZER_OFF){
        g_melody_playing = false;
        softToneWrite(pin, 0);
        digitalWrite(pin, LOW);
    }
    return 0;
}

void buzzer_exit(){
    // exit할게 있나..?
    SYSTEM_LOG("[LIBRARY] BUZZER exit %d", pin);
    softToneStop(pin);
    return;
}

DeviceInterface device_ops = {
    .init = buzzer_init,
    .control = buzzer_control,
    .read = NULL,
    .exit = buzzer_exit
};
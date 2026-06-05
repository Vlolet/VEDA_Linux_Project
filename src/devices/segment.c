#include <stdio.h>
#include <wiringPi.h>
#include <softPwm.h>

#include "common.h"
#include "device_interface.h"

int pins[8];
int digitMap[][8] = { // Cathode Map
    {1, 1, 1, 1, 1, 1, 0, 0}, // 0
    {0, 1, 1, 0, 0, 0, 0, 0}, // 1
    {1, 1, 0, 1, 1, 0, 1, 0}, // 2
    {1, 1, 1, 1, 0, 0, 1, 0}, // 3
    {0, 1, 1, 0, 0, 1, 1, 0}, // 4
    {1, 0, 1, 1, 0, 1, 1, 0}, // 5
    {1, 0, 1, 1, 1, 1, 1, 0}, // 6
    {1, 1, 1, 0, 0, 0, 0, 0}, // 7
    {1, 1, 1, 1, 1, 1, 1, 0}, // 8
    {1, 1, 1, 0, 0, 1, 1, 0} // 9
};
static const int is_anode = 1;

int segment_init(void* params){
    int *ps = (int*) params;
    // wiringPiSetupGpio 함수는 device_manager.c로 이동함
    // wiringPiSetupGpio();
    for(int i=0; i<8; i++){
        pins[i] = ps[i];
        pinMode(pins[i], OUTPUT);
    }
    
    SYSTEM_LOG("[LIBRARY] SEGMENT initialized on Pins %d %d %d %d %d %d %d %d",
         pins[0], pins[1], pins[2], pins[3], pins[4], pins[5], pins[6], pins[7]);
    return 0;
}

int segment_control(int cmd, int val){
    SYSTEM_LOG("[LIBRARY] segment cmd %d, val %d", cmd, val);
    if(cmd == CMD_SEGMENT_SET_DIGIT){
        if(val < 0 || 9 < val) return -1;

        if(is_anode){
            for(int i=0; i<8; i++){
            digitalWrite(pins[i], !digitMap[val][i]);
            }
        }

        else{
            for(int i=0; i<8; i++){
                digitalWrite(pins[i], digitMap[val][i]);
            }
        }
    }
    else if(cmd == CMD_SEGMENT_CLEAR){
        for(int i=0; i<8; i++){
            digitalWrite(pins[i], is_anode ? HIGH : LOW);
        }
    }
    else if(cmd == CMD_SEGMENT_SET_RAW){
        int target_idx = val;
        if('a' <= target_idx && target_idx <= 'h'){
            target_idx -= 'a';
        }

        if (target_idx >= 0 && target_idx < 8) {
            SYSTEM_LOG("[LIBRARY-RAW] target_idx: %d, BCM Pin: %d, Write Level: %s", 
                       target_idx, pins[target_idx], is_anode ? "LOW" : "HIGH");
        } else {
            SYSTEM_LOG("[LIBRARY-RAW] INVALID target_idx detected: %d (raw val: %d)", target_idx, val);
        }
        
        for(int i=0; i<8; i++){
            if(i == target_idx)
                digitalWrite(pins[i], is_anode ? LOW : HIGH);
            else
                digitalWrite(pins[i], is_anode ? HIGH : LOW);
        }
    }
    return 0;
}

void segment_exit(){
    // exit할게 있나..?
    SYSTEM_LOG("[LIBRARY] SEGMENT exit Pins %d %d %d %d %d %d %d %d",
         pins[0], pins[1], pins[2], pins[3], pins[4], pins[5], pins[6], pins[7]);
    return;
}

DeviceInterface device_ops = {
    .init = segment_init,
    .control = segment_control,
    .read = NULL,
    .exit = segment_exit
};
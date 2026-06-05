#include <stdio.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>

#include "common.h"
#include "device_interface.h"

int addr;
int fd = -1;
int a2dChannel = 0;

int cds_init(void* params){
    addr = *(int*)params;

    if((fd = wiringPiI2CSetup(addr)) < 0){
        SYSTEM_LOG("[LIBRARY] CDS wiringPiI2CSetup failed addr:%x", addr);
        return -1;
    }
    if((fd = wiringPiI2CSetupInterface("/dev/i2c-1", addr)) < 0){
        SYSTEM_LOG("[LIBRARY] CDS wiringPiI2CSetupInterface failed addr:%x, fd:%d", addr, fd);
        return -1;
    }
    
    SYSTEM_LOG("[LIBRARY] CDS initialized on addr %x", addr);
    return 0;
}

int cds_read(void* buf){
    // SYSTEM_LOG("[LIBRARY] CDS read called");
    if(fd < 0){
        SYSTEM_LOG("[LIBRARY] CDS device not initialized");
        return -1;
    }

    int channel = a2dChannel;
    wiringPiI2CWrite(fd, 0x00 | channel);
    wiringPiI2CRead(fd); // 직전 값 버리기
    
    int readVal = wiringPiI2CRead(fd);
    if(buf != NULL) *(int*)buf = readVal;
    return readVal;
}

void cds_exit(){
    // exit할게 있나..?
    SYSTEM_LOG("[LIBRARY] CDS exit %x", addr);
    fd = -1;
    return;
}

DeviceInterface device_ops = {
    .init = cds_init,
    .control = NULL,
    .read = cds_read,
    .exit = cds_exit
};
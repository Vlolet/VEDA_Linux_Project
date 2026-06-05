/** 동적 라이브러리 관리 및 스레드 핸들 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <pthread.h>
#include <wiringPi.h>

#include "common.h"
#include "config.h"
#include "device_interface.h"

typedef struct {
    char lib_name[MAX_LIB_NAME];
    void *handle;
    DeviceInterface *ops;
    pthread_mutex_t mutex;
} DeviceSlot;

static DeviceSlot led_dev = {"lib/libled.so", NULL, NULL, PTHREAD_MUTEX_INITIALIZER};
static DeviceSlot buzzer_dev = {"lib/libbuzzer.so", NULL, NULL, PTHREAD_MUTEX_INITIALIZER};
static DeviceSlot cds_dev = {"lib/libcds.so", NULL, NULL, PTHREAD_MUTEX_INITIALIZER};
static DeviceSlot segment_dev = {"lib/libsegment.so", NULL, NULL, PTHREAD_MUTEX_INITIALIZER};

static DeviceSlot *device_slots[] = {
    [TYPE_LED] = &led_dev, 
    [TYPE_BUZZER] = &buzzer_dev, 
    [TYPE_CDS] = &cds_dev, 
    [TYPE_SEGMENT] = &segment_dev
};

static int load_device(DeviceSlot *slot){
    char abs_dl_path[MAX_PATH];
    MAKE_ABSOLUTE_PATH(abs_dl_path, sizeof(abs_dl_path), slot->lib_name);

    pthread_mutex_lock(&(slot->mutex));

    // 핫스왑 시 기존 핸들 닫기 (exit: 라이브러리 내에 선언한 닫기 함수)
    if(slot->handle){
        SYSTEM_LOG("[load_device] close handle %s", slot->lib_name);
        if (slot->ops && slot->ops->exit){
            slot->ops->exit();
        }
        dlclose(slot->handle);
        slot->handle = NULL;
        slot->ops = NULL;
    }

    // 라이브러리 로드
    slot->handle = dlopen(abs_dl_path, RTLD_LAZY);
    if(!(slot->handle)){
        SYSTEM_LOG("FAILED: dlopen(%s) - %s", abs_dl_path, dlerror());
        pthread_mutex_unlock(&(slot->mutex));
        return -1;
    }

    // 구조체 심볼 찾기
    slot->ops = (DeviceInterface *) dlsym(slot->handle, "device_ops");
    if(!slot->ops){
        SYSTEM_LOG("FAILED: dlsym(device_ops) for %s", slot->lib_name);
        dlclose(slot->handle);
        slot->handle = NULL;
        pthread_mutex_unlock(&(slot->mutex));
        return -1;
    }

    // 장치 초기화(pin 정보 & pinMode 등)
    if(slot->ops->init){
        int init_res = 0;
        if(slot == &led_dev) init_res = slot->ops->init(&g_cfg.hw.led_gpio_pin);
        else if(slot == &buzzer_dev) init_res = slot->ops->init(&g_cfg.hw.buzzer_gpio_pin);
        else if(slot == &cds_dev) init_res = slot->ops->init(&g_cfg.hw.cds_i2c_addr);
        else if(slot == &segment_dev) init_res = slot->ops->init(g_cfg.hw.segment_gpio_pins);

        if(init_res != 0){
            SYSTEM_LOG("FAILED: hardware init failed in library %s", slot->lib_name);
            dlclose(slot->handle);
            slot->handle = NULL;
            slot->ops = NULL;
            pthread_mutex_unlock(&(slot->mutex));
            return -1;
        }
    }

    SYSTEM_LOG("SUCCESS: load_device %s", slot->lib_name);
    pthread_mutex_unlock(&(slot->mutex));
    return 0;
}

int init_device_manager(){
    if(wiringPiSetupGpio() == -1){
        SYSTEM_LOG("FAILED: wiringPi GPIO mode");
        return -1;
    }
    if(load_device(&led_dev) != 0) return -1;
    if(load_device(&buzzer_dev) != 0) return -1;
    if(load_device(&cds_dev) != 0) return -1;
    if(load_device(&segment_dev) != 0) return -1;
    return 0;
}

// 외부 스레드에서 장치 사용 요청. 장치 조작 함수 포인터 반환
DeviceInterface* lock_and_get_device(DeviceType type){
    if (type < 0 || type >= (sizeof(device_slots)/sizeof(device_slots[0]))){
        SYSTEM_LOG("ERROR: Invalid DeviceType %d", type);
        return NULL;
    }

    DeviceSlot *slot = device_slots[type];
    if (slot->handle == NULL || slot->ops == NULL){
        SYSTEM_LOG("ERROR: Device %s(%d) not loaded", slot->lib_name, type);
        return NULL;
    }

    pthread_mutex_lock(&(slot->mutex));
    return slot->ops;
}

// 장치를 쓰고 나서 호출. 라이브러리 업데이트 가능케 함
void release_device(DeviceType type){
    if (type < 0 || type >= (sizeof(device_slots)/sizeof(device_slots[0]))) return;

    pthread_mutex_unlock(&(device_slots[type]->mutex));
}

// 핫스왑을 위한 라이브러리 리로드 함수
void reload_devices(){
    SYSTEM_LOG("Hot-swap: Reloading libraries started...");

    load_device(&led_dev);
    load_device(&buzzer_dev);
    load_device(&cds_dev);
    load_device(&segment_dev);

    SYSTEM_LOG("Hot-swap: Reloading libraries finished.");
}

// 서버가 완전히 종료될 때 호출
void cleanup_devices(){
    int num_slots = sizeof(device_slots)/sizeof(device_slots[0]);

    for(int i=0; i < num_slots; i++){
        DeviceSlot *slot = device_slots[i];

        if(slot->handle){
            pthread_mutex_lock(&(slot->mutex));

            SYSTEM_LOG("[cleanup] Closing %s", slot->lib_name);

            if(slot->ops && slot->ops->exit){
                slot->ops->exit();
            }
            dlclose(slot->handle);
            slot->handle = NULL;
            slot->ops = NULL;

            pthread_mutex_unlock(&(slot->mutex));
            pthread_mutex_destroy(&(slot->mutex));
        }
    }

    return;
}
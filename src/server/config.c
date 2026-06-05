/** config.ini 파싱 */
// TODO: hw_map 쓰도록 리팩토링
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <libgen.h>

#include "common.h"
#include "config.h"

static char g_base_path[MAX_BASE_PATH] = {0};

void init_base_path(){
    char buf[MAX_BASE_PATH] = {0};
    char temp[MAX_BASE_PATH] = {0};
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if(len != -1){
        buf[len] = '\0';
        strcpy(temp, buf);
        char *dir = dirname(temp);
        strncpy(g_base_path, dir, sizeof(g_base_path) -1 );
        SYSTEM_LOG("PATH: %s", g_base_path);
    }
    else {
        strncpy(g_base_path, ".", sizeof(g_base_path) - 1);
        SYSTEM_LOG("PATH: (BASE) %s", g_base_path);
    }

    return;
}

const char* get_base_path(){
    return g_base_path;
}

ServerConfig g_cfg;

typedef struct {
    char *key;
    void *ptr;
} ConfigMap;

ConfigMap hw_map[] = {
    {"LED_GPIO_PIN", &g_cfg.hw.led_gpio_pin},
    {"BUZZER_GPIO_PIN", &g_cfg.hw.buzzer_gpio_pin},
    {"CDS_I2C_ADDR", &g_cfg.hw.cds_i2c_addr},
    {"SEGMENT_GPIO_PINS", &g_cfg.hw.segment_gpio_pins}
};

#define HW_MAP_SIZE (sizeof(hw_map) / sizeof(ConfigMap))

typedef enum {
    SECTION_NONE,
    SECTION_NETWORK,
    SECTION_HARDWARE
} IniSection;

int load_config(const char* filename){
    memset(&g_cfg, 0, sizeof(ServerConfig));
    int *hw_ptr = (int*) &g_cfg.hw;
    for(int i=0; i < sizeof(HWConfig)/sizeof(int); i++){
        hw_ptr[i] = -1;
    }

    FILE* fp = fopen(filename, "r");
    if(!fp){
        SYSTEM_LOG("FAILED: config_fopen(%s)", filename);
        return -1;
    }

    char line[128];
    IniSection curr_section = SECTION_NONE;

    while(fgets(line, sizeof(line), fp)){
        // 공백 & 개행 트리밍
        line[strcspn(line, "\r\n")] = 0;

        // 주석 & 빈 줄 스킵
        if(line[0] == '#' || line[0] == ';' || line[0] == '\0') continue;

        // 섹션 확인
        if(line[0] == '[' && strchr(line, ']')){
            if(strstr(line, "[Network]")) curr_section = SECTION_NETWORK;
            else if(strstr(line, "[Hardware]")) curr_section = SECTION_HARDWARE;
            continue;
        }

        char *key = strtok(line, "=");
        char *val = strtok(NULL, "=");
        if(!key || !val) continue;

        switch(curr_section){
            case SECTION_NETWORK:
                if(strcmp(key, "SERVER_IP") == 0) strcpy(g_cfg.net.ip, val);
                else if(strcmp(key, "SERVER_PORT") == 0) g_cfg.net.port = atoi(val);
                break;
            
            case SECTION_HARDWARE:
                // 여러 값을 가지는 경우
                if(strcmp(key, "SEGMENT_GPIO_PINS") == 0){
                    char *p = strtok(val, ",");
                    int idx = 0;
                    while(p != NULL && idx < 8){
                        g_cfg.hw.segment_gpio_pins[idx++] = atoi(p);
                        p = strtok(NULL, ",");
                    }
                }
                
                // 16진수 주소인 경우
                else if(strcmp(key, "CDS_I2C_ADDR") == 0){
                    int i2c_addr = (int)strtol(val, NULL, 0);
                    if(0x00 <= i2c_addr && i2c_addr <= 0x7F){
                        g_cfg.hw.cds_i2c_addr = i2c_addr;
                    }
                    else{
                        SYSTEM_LOG("FAILED: I2C addr is INVALID. check i2c addr: {i2cdetect -y 1}");
                        fclose(fp);
                        return -1;
                    }
                }

                // 단일 핀인 경우
                else{
                    for(int i=0; i < HW_MAP_SIZE; i++){
                        if(strcmp(key, hw_map[i].key) == 0){
                            *((int *)(hw_map[i].ptr)) = atoi(val);
                            break;
                        }
                    }
                }
                break;
            
            default: break;
        }
    }
    fclose(fp);

    int *chk_ptr = (int*) &g_cfg.hw;
    for(int i=0; i<sizeof(HWConfig)/sizeof(int); i++){
        if(chk_ptr[i] == -1){
            SYSTEM_LOG("FAILED: Missing config value at HW index %d", i);
            return -1;
        }
    }

    return 0;
}
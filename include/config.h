#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    char ip[20];
    int port;
} NetConfig;

typedef struct {
    int led_gpio_pin;
    int buzzer_gpio_pin;
    int cds_i2c_addr;
    int segment_gpio_pins[8];
    // pins...
} HWConfig;

typedef struct {
    NetConfig net;
    HWConfig hw;
} ServerConfig;

extern ServerConfig g_cfg;

int load_config(const char* filename);

#endif
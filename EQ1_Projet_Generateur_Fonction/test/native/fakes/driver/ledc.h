#ifndef DRIVER_LEDC_H
#define DRIVER_LEDC_H
#include <stdint.h>
#include "esp_err.h"
#define LEDC_LOW_SPEED_MODE 0
#define LEDC_USE_XTAL_CLK 3
#define LEDC_INTR_DISABLE 0
typedef int ledc_channel_t;
typedef int ledc_timer_t;
typedef int ledc_timer_bit_t;
typedef struct {int speed_mode,timer_num,duty_resolution;uint32_t freq_hz;int clk_cfg;} ledc_timer_config_t;
typedef struct {int gpio_num,speed_mode,channel,timer_sel,intr_type;uint32_t duty,hpoint;} ledc_channel_config_t;
esp_err_t ledc_timer_config(const ledc_timer_config_t *c);
uint32_t ledc_get_freq(int mode,int timer);
esp_err_t ledc_channel_config(const ledc_channel_config_t *c);
esp_err_t ledc_stop(int mode,int channel,int level);
#endif

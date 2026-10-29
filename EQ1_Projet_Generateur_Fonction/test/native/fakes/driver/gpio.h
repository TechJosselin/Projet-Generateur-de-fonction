#ifndef DRIVER_GPIO_H
#define DRIVER_GPIO_H
#include <stdint.h>
#include "esp_err.h"
typedef int gpio_num_t;
#define GPIO_IS_VALID_OUTPUT_GPIO(x) ((x)>=0 && (x)<31)
#define GPIO_NUM_0 0
#define GPIO_NUM_1 1
#define GPIO_NUM_2 2
#define GPIO_NUM_15 15
#define GPIO_NUM_18 18
#define GPIO_NUM_19 19
#define GPIO_NUM_21 21
#define GPIO_NUM_22 22
#define GPIO_NUM_23 23
#define GPIO_MODE_INPUT 1
#define GPIO_MODE_OUTPUT 2
#define GPIO_PULLUP_DISABLE 0
#define GPIO_PULLUP_ENABLE 1
#define GPIO_PULLDOWN_DISABLE 0
#define GPIO_INTR_DISABLE 0
#define GPIO_INTR_ANYEDGE 3
typedef struct {uint64_t pin_bit_mask;int mode,pull_up_en,pull_down_en,intr_type;} gpio_config_t;
esp_err_t gpio_set_level(int p,int v);
esp_err_t gpio_config(const gpio_config_t *c);
int gpio_get_level(int p);
esp_err_t gpio_install_isr_service(int flags);
esp_err_t gpio_isr_handler_add(int p,void (*fn)(void*),void *arg);
esp_err_t gpio_isr_handler_remove(int p);
#endif

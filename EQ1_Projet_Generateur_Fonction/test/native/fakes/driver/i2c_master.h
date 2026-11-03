#ifndef DRIVER_I2C_H
#define DRIVER_I2C_H
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
typedef void *i2c_master_bus_handle_t;
typedef struct fake_device *i2c_master_dev_handle_t;
#define I2C_NUM_0 0
#define I2C_ADDR_BIT_LEN_7 0
typedef struct {unsigned dev_addr_length,device_address,scl_speed_hz;} i2c_device_config_t;
esp_err_t i2c_master_probe(i2c_master_bus_handle_t b,uint16_t a,int ms);
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t b,const i2c_device_config_t *c,i2c_master_dev_handle_t *d);
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t d);
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t d,const uint8_t *b,size_t n,int ms);
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t d,const uint8_t *b,size_t n,uint8_t *out,size_t m,int ms);
#endif

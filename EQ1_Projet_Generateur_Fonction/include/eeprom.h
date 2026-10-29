#ifndef EEPROM_H
#define EEPROM_H
#include "stockage.h"
#include "driver/i2c_master.h"
esp_err_t eeprom_initialiser(i2c_master_bus_handle_t bus,uint8_t adresse);
stockage_io_t eeprom_interface(void);
#endif

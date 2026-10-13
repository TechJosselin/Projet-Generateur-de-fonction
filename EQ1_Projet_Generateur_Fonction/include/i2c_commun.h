#ifndef I2C_COMMUN_H
#define I2C_COMMUN_H

#include "driver/i2c_master.h"
#include "esp_err.h"

/* Bus I2C unique partage entre OLED, EEPROM et EPOT. */
esp_err_t i2c_commun_initialiser(void);
esp_err_t i2c_commun_desinitialiser(void);
i2c_master_bus_handle_t i2c_commun_bus(void);

#endif /* I2C_COMMUN_H */

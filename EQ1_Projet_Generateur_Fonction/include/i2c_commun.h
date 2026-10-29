#ifndef I2C_COMMUN_H
#define I2C_COMMUN_H

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

/* Bus I2C unique partage entre OLED, EEPROM et EPOT. */
esp_err_t i2c_commun_initialiser(void);
esp_err_t i2c_commun_desinitialiser(void);
i2c_master_bus_handle_t i2c_commun_bus(void);
bool i2c_commun_est_initialise(void);

/* Diagnostic non destructif : vérifie qu'une adresse répond par ACK. */
bool i2c_commun_adresse_presente(uint8_t adresse, int timeout_ms);

/* Cherche la première adresse qui répond dans une plage inclusive. */
bool i2c_commun_trouver_adresse(uint8_t adresse_min,
                                uint8_t adresse_max,
                                int timeout_ms,
                                uint8_t *adresse_trouvee);

#endif /* I2C_COMMUN_H */

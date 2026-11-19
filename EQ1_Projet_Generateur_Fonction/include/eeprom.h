/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : EEPROM.H
 * DESCRIPTIF : Communication I2C et acces a la memoire 24LC32.
 *==================================================================================================*/
#ifndef EEPROM_H
#define EEPROM_H
#include "stockage.h"
#include "driver/i2c_master.h"

/*============================== VARIABLES ET CONFIGURATION =======================================*/
esp_err_t eeprom_initialiser(i2c_master_bus_handle_t bus, uint8_t adresse);
stockage_io_t eeprom_interface(void);
#endif

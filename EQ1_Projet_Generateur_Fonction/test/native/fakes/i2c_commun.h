/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : I2C_COMMUN.H
 * DESCRIPTIF : Declarations du faux bus I2C utilise par les pilotes simules.
 *==================================================================================================*/
#ifndef FAKE_I2C_COMMUN_H_INCLUDED
#define FAKE_I2C_COMMUN_H_INCLUDED

#include "driver/i2c_master.h"

/*================================= DONNEES DE SIMULATION =========================================*/
esp_err_t i2c_commun_initialiser(void);
i2c_master_bus_handle_t i2c_commun_bus(void);

#endif /* FAKE_I2C_COMMUN_H_INCLUDED */

/*===============================================================================================
CFPT - Projet : Générateur de fonction
Fichier      : oled.h
Description  : Gestion de l'écran OLED SH1106 128x64 I2C
===============================================================================================*/

#ifndef OLED_H
#define OLED_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "driver/i2c_master.h"

esp_err_t Oled_Init(i2c_master_bus_handle_t bus_handle);
esp_err_t Oled_Deinit(void);
esp_err_t Oled_Clear(void);
esp_err_t Oled_TestPattern(void);
esp_err_t Oled_ShowCounter(int32_t value);

bool Oled_IsInitialized(void);
uint8_t Oled_GetI2cAddress(void);

#endif /* OLED_H */

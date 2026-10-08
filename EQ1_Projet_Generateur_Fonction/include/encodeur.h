/*===============================================================================================
CFPT - Projet : Générateur de fonction
Fichier      : encodeur.h
Description  : Gestion de l'encodeur rotatif PEC12R sous interruption GPIO
===============================================================================================*/

#ifndef ENCODEUR_H
#define ENCODEUR_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "driver/gpio.h"

esp_err_t Encodeur_Init(gpio_num_t pin_a,
                        gpio_num_t pin_b,
                        gpio_num_t pin_button,
                        uint8_t edges_per_step,
                        bool reverse_direction);

esp_err_t Encodeur_Deinit(void);

int32_t Encodeur_GetCounter(void);
esp_err_t Encodeur_SetCounter(int32_t value);

bool Encodeur_IsInitialized(void);
bool Encodeur_IsButtonPressed(void);

#endif /* ENCODEUR_H */

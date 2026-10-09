/*===============================================================================================
CFPT - Projet : Générateur de fonction
Fichier      : signaux.h
Description  : Modèle de signal et gestion des presets du générateur
===============================================================================================*/

#ifndef SIGNAUX_H
#define SIGNAUX_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define SIGNAUX_PRESET_COUNT                    10U

typedef enum
{
    SIGNAUX_TYPE_SINUS = 0,
    SIGNAUX_TYPE_TRIANGLE,
    SIGNAUX_TYPE_CARRE
} signaux_type_t;

typedef struct
{
    signaux_type_t type;
    float frequency_hz;
    float amplitude_vpp;
    float offset_v;
} signaux_signal_t;

esp_err_t Signaux_GetPreset(uint8_t preset_number, signaux_signal_t *out_signal);
esp_err_t Signaux_Apply(const signaux_signal_t *signal);
esp_err_t Signaux_ApplyPreset(uint8_t preset_number);

/*
 * Commande brute du potentiomètre de gain.
 * Cette fonction permet de tester/intégrer l'EPOT avant la calibration
 * amplitude Vpp -> valeur de wiper.
 */
esp_err_t Signaux_SetGainRaw(uint8_t raw_value);
esp_err_t Signaux_GetGainRaw(uint8_t *out_raw_value);

bool Signaux_HasCurrentSignal(void);
esp_err_t Signaux_GetCurrent(signaux_signal_t *out_signal);
const char *Signaux_TypeToString(signaux_type_t type);

#endif /* SIGNAUX_H */

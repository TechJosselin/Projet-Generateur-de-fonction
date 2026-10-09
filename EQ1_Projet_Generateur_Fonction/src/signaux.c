/*===============================================================================================
CFPT - Projet : Générateur de fonction
Fichier      : signaux.c
Description  : Gestion du signal complet et des 10 presets du cahier des charges

Base d'intégration : presets et génération de Samuel (RDMR_GenerationSignal).
L'AD9833 applique actuellement la forme et la fréquence. L'amplitude et l'offset restent mémorisés
ici jusqu'à la phase de calibration de la chaîne analogique / des EPOT.
===============================================================================================*/

#include "signaux.h"

#include <stddef.h>

#include "ad9833.h"

static const signaux_signal_t gPresets[SIGNAUX_PRESET_COUNT] =
{
    { SIGNAUX_TYPE_SINUS,    10000.0f,   1.0f,   0.0f },
    { SIGNAUX_TYPE_SINUS,  1000000.0f,   5.0f,   0.0f },
    { SIGNAUX_TYPE_SINUS,     1000.0f,   2.0f,   3.0f },
    { SIGNAUX_TYPE_SINUS,      500.0f,   0.2f,  10.0f },
    { SIGNAUX_TYPE_TRIANGLE,  5000.0f,  20.0f,   0.0f },
    { SIGNAUX_TYPE_TRIANGLE,   100.0f,   4.0f,   0.0f },
    { SIGNAUX_TYPE_TRIANGLE,200000.0f,   5.0f,  -5.0f },
    { SIGNAUX_TYPE_CARRE,       50.0f,  10.0f,   0.0f },
    { SIGNAUX_TYPE_CARRE,      300.0f,   1.0f,   0.0f },
    { SIGNAUX_TYPE_CARRE,     1000.0f,   4.0f,   6.0f },
};

static signaux_signal_t gCurrentSignal;
static bool gHasCurrentSignal = false;

static esp_err_t Signaux_ToAd9833Waveform(signaux_type_t type, ad9833_waveform_t *waveform)
{
    if (waveform == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    switch (type)
    {
        case SIGNAUX_TYPE_SINUS:
            *waveform = AD9833_WAVE_SINE;
            return ESP_OK;

        case SIGNAUX_TYPE_TRIANGLE:
            *waveform = AD9833_WAVE_TRIANGLE;
            return ESP_OK;

        case SIGNAUX_TYPE_CARRE:
            *waveform = AD9833_WAVE_SQUARE;
            return ESP_OK;

        default:
            return ESP_ERR_INVALID_ARG;
    }
}

esp_err_t Signaux_GetPreset(uint8_t preset_number, signaux_signal_t *out_signal)
{
    if ((out_signal == NULL) ||
        (preset_number == 0U) ||
        (preset_number > SIGNAUX_PRESET_COUNT))
    {
        return ESP_ERR_INVALID_ARG;
    }

    *out_signal = gPresets[preset_number - 1U];
    return ESP_OK;
}

esp_err_t Signaux_Apply(const signaux_signal_t *signal)
{
    esp_err_t err;
    ad9833_waveform_t waveform;

    if (signal == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if ((signal->frequency_hz < 0.0f) || (signal->amplitude_vpp < 0.0f))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!Ad9833_IsInitialized())
    {
        return ESP_ERR_INVALID_STATE;
    }

    err = Signaux_ToAd9833Waveform(signal->type, &waveform);
    if (err != ESP_OK)
    {
        return err;
    }

    err = Ad9833_SetSignal(waveform, (double)signal->frequency_hz);
    if (err != ESP_OK)
    {
        return err;
    }

    /*
     * amplitude_vpp et offset_v sont volontairement conservés dans le modèle du signal mais ne
     * sont pas encore convertis en valeurs brutes pour les deux MCP45HV51. Cette conversion sera
     * ajoutée après caractérisation/calibration de la chaîne analogique.
     */
    gCurrentSignal = *signal;
    gHasCurrentSignal = true;

    return ESP_OK;
}

esp_err_t Signaux_ApplyPreset(uint8_t preset_number)
{
    signaux_signal_t signal;
    esp_err_t err = Signaux_GetPreset(preset_number, &signal);

    if (err != ESP_OK)
    {
        return err;
    }

    return Signaux_Apply(&signal);
}

bool Signaux_HasCurrentSignal(void)
{
    return gHasCurrentSignal;
}

esp_err_t Signaux_GetCurrent(signaux_signal_t *out_signal)
{
    if (out_signal == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!gHasCurrentSignal)
    {
        return ESP_ERR_INVALID_STATE;
    }

    *out_signal = gCurrentSignal;
    return ESP_OK;
}

const char *Signaux_TypeToString(signaux_type_t type)
{
    switch (type)
    {
        case SIGNAUX_TYPE_SINUS:    return "SINUS";
        case SIGNAUX_TYPE_TRIANGLE: return "TRIANGLE";
        case SIGNAUX_TYPE_CARRE:    return "CARRE";
        default:                    return "INCONNU";
    }
}

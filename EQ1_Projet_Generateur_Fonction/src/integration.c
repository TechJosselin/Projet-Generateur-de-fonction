#include "integration.h"

#include "ad9833.h"
#include "epot.h"

static int selection = -1;

static bool convertir_forme(signal_forme_t forme, ad9833_waveform_t *waveform)
{
    if (waveform == NULL) return false;
    switch (forme)
    {
        case SIGNAL_SINUS: *waveform = AD9833_WAVE_SINE; return true;
        case SIGNAL_TRIANGLE: *waveform = AD9833_WAVE_TRIANGLE; return true;
        case SIGNAL_CARRE: *waveform = AD9833_WAVE_SQUARE; return true;
        default: return false;
    }
}

void integration_initialiser(void)
{
    selection = -1;
}

integration_selection_t integration_selectionner(size_t index, const signal_config_t *config)
{
    ad9833_waveform_t waveform;
    if ((index >= signaux_nombre()) || (config == NULL) || !signaux_valide(*config))
        return INTEGRATION_ECHEC;

    selection = (int)index;

    if (!Ad9833_IsInitialized())
        return INTEGRATION_SELECTION_RAM;

    if (!convertir_forme(config->forme, &waveform))
        return INTEGRATION_ECHEC;

    if (Ad9833_SetSignal(waveform, (double)config->frequence_hz) != ESP_OK)
        return INTEGRATION_ECHEC;

    return INTEGRATION_SELECTION_DDS_PREPAREE;
}

int integration_signal_selectionne(void) { return selection; }

void integration_annuler_selection(void)
{
    selection = -1;
    if (Ad9833_IsInitialized()) (void)Ad9833_EnableOutput(false);
}

bool integration_ad9833_disponible(void) { return Ad9833_IsInitialized(); }
bool integration_epot_disponible(void) { return Epot_IsInitialized(); }
bool integration_sortie_reelle_active(void) { return false; }
bool integration_activer_sortie(void) { return false; }
bool integration_couper_sortie(void)
{
    if (Ad9833_IsInitialized()) return Ad9833_EnableOutput(false) == ESP_OK;
    return true;
}
bool integration_memoire_disponible(void) { return false; }
bool integration_eeprom_sauvegarder(void) { return false; }
bool integration_eeprom_charger(void) { return false; }
bool integration_usb_trames_disponible(void) { return false; }
bool integration_sortie_digitale_disponible(unsigned canal) { (void)canal; return false; }

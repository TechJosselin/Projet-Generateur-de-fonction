#include "integration.h"

#include "ad9833.h"
#include "epot.h"
#include "hardware_config.h"
#include "i2c_commun.h"

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

int integration_signal_selectionne(void)
{
    return selection;
}

void integration_annuler_selection(void)
{
    selection = -1;
    if (Ad9833_IsInitialized())
    {
        (void)Ad9833_EnableOutput(false);
    }
}

bool integration_i2c_disponible(void)
{
    return i2c_commun_est_initialise();
}

bool integration_epot_gain_present(void)
{
    return i2c_commun_adresse_presente(SYSTEM_EPOT_GAIN_I2C_ADDRESS,
                                       SYSTEM_I2C_DIAG_PROBE_TIMEOUT_MS);
}

bool integration_epot_offset_present(void)
{
    return i2c_commun_adresse_presente(SYSTEM_EPOT_OFFSET_I2C_ADDRESS,
                                       SYSTEM_I2C_DIAG_PROBE_TIMEOUT_MS);
}

bool integration_eeprom_detectee(uint8_t *adresse)
{
    const bool presente = i2c_commun_adresse_presente(SYSTEM_EEPROM_I2C_ADDRESS,
                                                       SYSTEM_I2C_DIAG_PROBE_TIMEOUT_MS);

    if (adresse != NULL)
    {
        *adresse = presente ? SYSTEM_EEPROM_I2C_ADDRESS : 0U;
    }

    return presente;
}

bool integration_ad9833_disponible(void)
{
    /* AD9833 sans MISO : on peut valider le driver SPI, pas obtenir d'ACK matériel. */
    return Ad9833_IsInitialized();
}

bool integration_epot_disponible(void)
{
    return Epot_IsInitialized() &&
           integration_epot_gain_present() &&
           integration_epot_offset_present();
}

bool integration_sortie_reelle_active(void)
{
    return Ad9833_IsInitialized() && Ad9833_IsOutputEnabled();
}

bool integration_activer_sortie(void)
{
    /* Bloqué tant que la conversion amplitude/offset et l'étage analogique ne sont pas calibrés. */
    return false;
}

bool integration_couper_sortie(void)
{
    if (Ad9833_IsInitialized())
    {
        return Ad9833_EnableOutput(false) == ESP_OK;
    }
    return true;
}

bool integration_memoire_disponible(void)
{
    /* La présence physique peut être diagnostiquée, mais le driver 24LC32 n'est pas encore intégré. */
    return false;
}

bool integration_eeprom_sauvegarder(void)
{
    return false;
}

bool integration_eeprom_charger(void)
{
    return false;
}

bool integration_usb_trames_disponible(void)
{
    /* Le parser existe, mais le transport USB RX/TX n'est pas encore raccordé. */
    return false;
}

bool integration_sortie_digitale_disponible(unsigned canal)
{
    (void)canal;
    return false;
}

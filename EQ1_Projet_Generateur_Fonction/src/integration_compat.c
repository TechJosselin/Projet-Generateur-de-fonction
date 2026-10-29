#include "integration_compat.h"
#include "ad9833.h"
#include "epot.h"
#include "usb_transport.h"
#include "hardware_config.h"
#include "i2c_commun.h"

/* Ne pas contourner la calibration en activant directement le DDS. */
integration_selection_t integration_selectionner(size_t i,const signal_config_t *s)
{
    const signal_config_t *original=signaux_lire(i);
    if(!s || !original || !signaux_egaux(*original,*s) || !signaux_valide(*s))
        return INTEGRATION_ECHEC;
    if(!integration_ad9833_disponible())
        return integration_choisir(i)==GF_OK?INTEGRATION_SELECTION_RAM:INTEGRATION_ECHEC;
    /* STOP avant toute reprogrammation. Sur echec SPI, l'ancienne selection
       n'est pas remplacee et le DDS n'est jamais ouvert. */
    if(integration_arreter()!=GF_OK)return INTEGRATION_ECHEC;
    if(Ad9833_SetSignal((ad9833_waveform_t)s->forme,(double)s->frequence_hz)!=ESP_OK)
        return INTEGRATION_ECHEC;
    return integration_choisir(i)==GF_OK?INTEGRATION_SELECTION_DDS_PREPAREE:INTEGRATION_ECHEC;
}
int integration_signal_selectionne(void){return integration_statut()->selection;}
void integration_annuler_selection(void)
{
    /* Les actions de suppression, usine et lecture EEPROM ont deja
       remis selection=-1 dans integration.c. On impose l'arret. */
    (void)integration_arreter();
}
bool integration_epot_gain_present(void){return Epot_IsChannelAvailable(EPOT_CHANNEL_GAIN);}
bool integration_epot_offset_present(void){return Epot_IsChannelAvailable(EPOT_CHANNEL_OFFSET);}
bool integration_eeprom_detectee(uint8_t *adresse)
{
    bool presente=i2c_commun_adresse_presente(SYSTEM_EEPROM_I2C_ADDRESS,
                                              SYSTEM_I2C_DIAG_PROBE_TIMEOUT_MS);
    if(adresse)*adresse=presente?SYSTEM_EEPROM_I2C_ADDRESS:0;
    return presente;
}
bool integration_sortie_reelle_active(void)
{
    return integration_statut()->etat==SYS_ACTIF_COMMANDE && Ad9833_IsOutputEnabled();
}
bool integration_activer_sortie(void){return integration_activer()==GF_OK;}
bool integration_couper_sortie(void){return integration_arreter()==GF_OK;}
/* Retour true = demande acceptée ; ecriture EEPROM asynchrone. */
bool integration_eeprom_sauvegarder(void){return integration_sauver()==GF_OK;}
bool integration_eeprom_charger(void){return integration_charger()==GF_OK;}
bool integration_usb_trames_disponible(void){return usb_transport_disponible();}

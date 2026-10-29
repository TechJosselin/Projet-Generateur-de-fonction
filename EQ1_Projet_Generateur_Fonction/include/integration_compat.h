#ifndef INTEGRATION_COMPAT_H
#define INTEGRATION_COMPAT_H
#include "integration.h"
/* Compatibilite des actions (pas de rendu graphique). */
typedef enum {
    INTEGRATION_SELECTION_RAM=0,
    INTEGRATION_SELECTION_DDS_PREPAREE=1,
    INTEGRATION_ECHEC=2
} integration_selection_t;
integration_selection_t integration_selectionner(size_t index,const signal_config_t *config);
int integration_signal_selectionne(void);
void integration_annuler_selection(void);
bool integration_epot_gain_present(void);
bool integration_epot_offset_present(void);
bool integration_eeprom_detectee(uint8_t *adresse);
bool integration_sortie_reelle_active(void);
bool integration_activer_sortie(void);
bool integration_couper_sortie(void);
bool integration_eeprom_sauvegarder(void);
bool integration_eeprom_charger(void);
bool integration_usb_trames_disponible(void);
#endif

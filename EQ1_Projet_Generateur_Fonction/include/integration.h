#ifndef INTEGRATION_H
#define INTEGRATION_H

#include "signaux.h"
#include <stdbool.h>
#include <stddef.h>

typedef enum
{
    INTEGRATION_SELECTION_RAM = 0,
    INTEGRATION_SELECTION_DDS_PREPAREE = 1,
    INTEGRATION_ECHEC = 2
} integration_selection_t;

void integration_initialiser(void);
integration_selection_t integration_selectionner(size_t index, const signal_config_t *config);
int integration_signal_selectionne(void);
void integration_annuler_selection(void);

bool integration_ad9833_disponible(void);
bool integration_epot_disponible(void);
bool integration_sortie_reelle_active(void);
bool integration_activer_sortie(void);
bool integration_couper_sortie(void);

bool integration_memoire_disponible(void);
bool integration_eeprom_sauvegarder(void);
bool integration_eeprom_charger(void);
bool integration_usb_trames_disponible(void);
bool integration_sortie_digitale_disponible(unsigned canal);

#endif /* INTEGRATION_H */

/*===============================================================================================
CFPT - Projet : Générateur de fonction
Fichier      : initialisation.h
Description  : Initialisation des ressources matérielles communes du système
===============================================================================================*/

#ifndef INITIALISATION_H
#define INITIALISATION_H

/*===============================================================================================
INCLUDES
===============================================================================================*/
#include <stdbool.h>

#include "esp_err.h"
#include "driver/i2c_master.h"

/*===============================================================================================
PROTOTYPES
===============================================================================================*/
/**
 * @brief Initialise les ressources matérielles communes du système.
 *
 * Cette fonction crée notamment l'unique bus I2C partagé par les différents
 * périphériques du projet. Elle peut être appelée plusieurs fois sans recréer
 * les ressources déjà initialisées.
 *
 * @return ESP_OK si l'initialisation a réussi, sinon un code d'erreur ESP-IDF.
 */
esp_err_t System_Init(void);

/**
 * @brief Libère les ressources matérielles communes du système.
 *
 * Les périphériques ajoutés au bus I2C doivent avoir été retirés avant cet appel.
 *
 * @return ESP_OK si la libération a réussi, sinon un code d'erreur ESP-IDF.
 */
esp_err_t System_Deinit(void);

/**
 * @brief Retourne le handle du bus I2C partagé.
 *
 * @return Handle du bus I2C, ou NULL si le système n'est pas initialisé.
 */
i2c_master_bus_handle_t System_GetI2cBus(void);

/**
 * @brief Indique si les ressources communes du système sont initialisées.
 */
bool System_IsInitialized(void);

#endif /* INITIALISATION_H */

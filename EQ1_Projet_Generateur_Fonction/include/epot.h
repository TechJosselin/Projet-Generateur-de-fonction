/*===============================================================================================
CFPT - Projet : Générateur de fonction
Fichier      : epot.h
Description  : Driver des potentiomètres numériques MCP45HV51 (Gain / Offset)
===============================================================================================*/

#ifndef EPOT_H
#define EPOT_H

/*===============================================================================================
INCLUDES
===============================================================================================*/
#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "driver/i2c_master.h"

/*===============================================================================================
DEFINES
===============================================================================================*/
#define EPOT_I2C_SPEED_HZ          100000U
#define EPOT_I2C_TIMEOUT_MS        100U

#define EPOT_I2C_ADDRESS_MIN       0x3CU
#define EPOT_I2C_ADDRESS_MAX       0x3FU

#define EPOT_RAW_MIN               0U
#define EPOT_RAW_MAX               255U

/*===============================================================================================
TYPES
===============================================================================================*/
typedef enum
{
    EPOT_CHANNEL_GAIN = 0,
    EPOT_CHANNEL_OFFSET,
    EPOT_CHANNEL_COUNT
} epot_channel_t;

/*===============================================================================================
PROTOTYPES
===============================================================================================*/
/**
 * @brief Enregistre les deux MCP45HV51 sur le bus I2C partagé.
 *
 * Cette fonction ne modifie pas les wipers : elle prépare uniquement les deux
 * périphériques pour les futures commandes de gain et d'offset.
 */
esp_err_t Epot_Init(i2c_master_bus_handle_t bus_handle,
                    uint8_t gain_address,
                    uint8_t offset_address);

/**
 * @brief Retire les deux MCP45HV51 du bus I2C.
 */
esp_err_t Epot_Deinit(void);

/**
 * @brief Ecrit directement la valeur brute 8 bits du wiper d'un canal.
 */
esp_err_t Epot_SetRaw(epot_channel_t channel, uint8_t value);
esp_err_t Epot_SetGainRaw(uint8_t value);
esp_err_t Epot_SetOffsetRaw(uint8_t value);

/**
 * @brief Retourne la dernière valeur brute écrite avec succès sur un canal.
 *
 * Aucune valeur n'est disponible juste après Epot_Init(), puisque le driver ne
 * force volontairement aucune position de wiper au démarrage.
 */
esp_err_t Epot_GetLastRaw(epot_channel_t channel, uint8_t *out_value);

bool Epot_IsInitialized(void);
bool Epot_IsChannelAvailable(epot_channel_t channel);

#endif /* EPOT_H */

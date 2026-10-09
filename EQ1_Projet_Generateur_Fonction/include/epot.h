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
    EPOT_CHANNEL_OFFSET
} epot_channel_t;

/*===============================================================================================
PROTOTYPES
===============================================================================================*/
esp_err_t Epot_Init(i2c_master_bus_handle_t bus_handle,
                    uint8_t gain_address,
                    uint8_t offset_address);

esp_err_t Epot_InitSingle(i2c_master_bus_handle_t bus_handle,
                          epot_channel_t channel,
                          uint8_t address);

esp_err_t Epot_Deinit(void);

esp_err_t Epot_SetRaw(epot_channel_t channel, uint8_t value);
esp_err_t Epot_SetGainRaw(uint8_t value);
esp_err_t Epot_SetOffsetRaw(uint8_t value);

bool Epot_IsInitialized(void);
bool Epot_IsChannelAvailable(epot_channel_t channel);

#endif /* EPOT_H */

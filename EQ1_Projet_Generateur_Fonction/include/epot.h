#ifndef EPOT_H
#define EPOT_H
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"
#define EPOT_I2C_SPEED_HZ 100000U
#define EPOT_I2C_TIMEOUT_MS 30U
#define EPOT_I2C_ADDRESS_MIN 0x3CU
#define EPOT_I2C_ADDRESS_MAX 0x3FU
#define EPOT_RAW_MIN 0U
#define EPOT_RAW_MAX 255U
typedef enum {EPOT_CHANNEL_GAIN=0,EPOT_CHANNEL_OFFSET,EPOT_CHANNEL_COUNT} epot_channel_t;
/* Configure TCON, ne choisit pas de position de curseur arbitraire au boot. */
esp_err_t Epot_Init(i2c_master_bus_handle_t bus,uint8_t gain_address,uint8_t offset_address);
esp_err_t Epot_Deinit(void);
/* Ecriture suivie d'une relecture de registre. Ne mesure pas la resistance. */
esp_err_t Epot_SetRaw(epot_channel_t channel,uint8_t value);
esp_err_t Epot_SetGainRaw(uint8_t value);
esp_err_t Epot_SetOffsetRaw(uint8_t value);
esp_err_t Epot_GetLastRaw(epot_channel_t channel,uint8_t *out_value);
esp_err_t Epot_ReadRaw(epot_channel_t channel,uint8_t *out_value);
bool Epot_IsInitialized(void);
bool Epot_IsChannelAvailable(epot_channel_t channel);
#endif

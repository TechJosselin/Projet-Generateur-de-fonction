/*===============================================================================================
CFPT - Projet : Générateur de fonction
Fichier      : epot.c
Description  : Driver des potentiomètres numériques MCP45HV51 (Gain / Offset)
===============================================================================================*/

/*===============================================================================================
INCLUDES
===============================================================================================*/
#include "epot.h"

/*===============================================================================================
DEFINES
===============================================================================================*/
#define EPOT_MEMORY_WIPER0         0x00U
#define EPOT_COMMAND_WRITE         0x00U

#define EPOT_BUILD_COMMAND(memory_address, command) \
    (uint8_t)((((memory_address) & 0x0FU) << 4U) | (((command) & 0x03U) << 2U))

/*===============================================================================================
VARIABLES LOCALES
===============================================================================================*/
static i2c_master_dev_handle_t gGainHandle = NULL;
static i2c_master_dev_handle_t gOffsetHandle = NULL;
static bool gEpotInitialized = false;

/*===============================================================================================
PROTOTYPES DE FONCTIONS LOCALES
===============================================================================================*/
static bool Epot_IsValidAddress(uint8_t address);
static esp_err_t Epot_AddDevice(i2c_master_bus_handle_t bus_handle,
                                uint8_t address,
                                i2c_master_dev_handle_t *device_handle);
static esp_err_t Epot_WriteWiper(i2c_master_dev_handle_t device_handle, uint8_t value);

/*===============================================================================================
FONCTIONS LOCALES
===============================================================================================*/
static bool Epot_IsValidAddress(uint8_t address)
{
    return (address >= EPOT_I2C_ADDRESS_MIN) && (address <= EPOT_I2C_ADDRESS_MAX);
}

static esp_err_t Epot_AddDevice(i2c_master_bus_handle_t bus_handle,
                                uint8_t address,
                                i2c_master_dev_handle_t *device_handle)
{
    if ((bus_handle == NULL) || (device_handle == NULL))
    {
        return ESP_ERR_INVALID_ARG;
    }

    const i2c_device_config_t device_config =
    {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = EPOT_I2C_SPEED_HZ,
    };

    return i2c_master_bus_add_device(bus_handle, &device_config, device_handle);
}

static esp_err_t Epot_WriteWiper(i2c_master_dev_handle_t device_handle, uint8_t value)
{
    if (device_handle == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    const uint8_t frame[2] =
    {
        EPOT_BUILD_COMMAND(EPOT_MEMORY_WIPER0, EPOT_COMMAND_WRITE),
        value
    };

    return i2c_master_transmit(device_handle,
                               frame,
                               sizeof(frame),
                               EPOT_I2C_TIMEOUT_MS);
}

/*===============================================================================================
FONCTIONS PUBLIQUES
===============================================================================================*/
esp_err_t Epot_Init(i2c_master_bus_handle_t bus_handle,
                    uint8_t gain_address,
                    uint8_t offset_address)
{
    esp_err_t err;

    if (bus_handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!Epot_IsValidAddress(gain_address) ||
        !Epot_IsValidAddress(offset_address) ||
        (gain_address == offset_address))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (gEpotInitialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    err = Epot_AddDevice(bus_handle, gain_address, &gGainHandle);
    if (err != ESP_OK)
    {
        gGainHandle = NULL;
        return err;
    }

    err = Epot_AddDevice(bus_handle, offset_address, &gOffsetHandle);
    if (err != ESP_OK)
    {
        i2c_master_bus_rm_device(gGainHandle);
        gGainHandle = NULL;
        gOffsetHandle = NULL;
        return err;
    }

    gEpotInitialized = true;
    return ESP_OK;
}

esp_err_t Epot_Deinit(void)
{
    esp_err_t first_error = ESP_OK;
    esp_err_t err;

    if (!gEpotInitialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (gGainHandle != NULL)
    {
        err = i2c_master_bus_rm_device(gGainHandle);
        if ((err != ESP_OK) && (first_error == ESP_OK))
        {
            first_error = err;
        }
        gGainHandle = NULL;
    }

    if (gOffsetHandle != NULL)
    {
        err = i2c_master_bus_rm_device(gOffsetHandle);
        if ((err != ESP_OK) && (first_error == ESP_OK))
        {
            first_error = err;
        }
        gOffsetHandle = NULL;
    }

    gEpotInitialized = false;
    return first_error;
}

esp_err_t Epot_SetRaw(epot_channel_t channel, uint8_t value)
{
    if (!gEpotInitialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    switch (channel)
    {
        case EPOT_CHANNEL_GAIN:
            return Epot_WriteWiper(gGainHandle, value);

        case EPOT_CHANNEL_OFFSET:
            return Epot_WriteWiper(gOffsetHandle, value);

        default:
            return ESP_ERR_INVALID_ARG;
    }
}

esp_err_t Epot_SetGainRaw(uint8_t value)
{
    return Epot_SetRaw(EPOT_CHANNEL_GAIN, value);
}

esp_err_t Epot_SetOffsetRaw(uint8_t value)
{
    return Epot_SetRaw(EPOT_CHANNEL_OFFSET, value);
}

bool Epot_IsInitialized(void)
{
    return gEpotInitialized;
}

/*===============================================================================================
FIN DU FICHIER
===============================================================================================*/

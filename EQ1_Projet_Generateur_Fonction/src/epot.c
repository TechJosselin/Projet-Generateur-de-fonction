/*===============================================================================================
CFPT - Projet : Générateur de fonction
Fichier      : epot.c
Description  : Driver des potentiomètres numériques MCP45HV51 (Gain / Offset)
===============================================================================================*/

/*===============================================================================================
INCLUDES
===============================================================================================*/
#include "epot.h"

#include <stddef.h>

/*===============================================================================================
DEFINES
===============================================================================================*/
#define EPOT_MEMORY_WIPER0         0x00U
#define EPOT_COMMAND_WRITE         0x00U

#define EPOT_BUILD_COMMAND(memory_address, command) \
    (uint8_t)((((memory_address) & 0x0FU) << 4U) | (((command) & 0x03U) << 2U))

/*===============================================================================================
TYPES LOCAUX
===============================================================================================*/
typedef struct
{
    i2c_master_dev_handle_t handle;
    uint8_t last_raw;
    bool has_last_raw;
} epot_device_t;

/*===============================================================================================
VARIABLES LOCALES
===============================================================================================*/
static epot_device_t gDevices[EPOT_CHANNEL_COUNT] = {0};
static bool gEpotInitialized = false;

/*===============================================================================================
PROTOTYPES DE FONCTIONS LOCALES
===============================================================================================*/
static bool Epot_IsValidAddress(uint8_t address);
static bool Epot_IsValidChannel(epot_channel_t channel);
static esp_err_t Epot_AddDevice(i2c_master_bus_handle_t bus_handle,
                                uint8_t address,
                                i2c_master_dev_handle_t *device_handle);
static esp_err_t Epot_WriteWiper(i2c_master_dev_handle_t device_handle, uint8_t value);
static void Epot_ResetDevice(epot_device_t *device);

/*===============================================================================================
FONCTIONS LOCALES
===============================================================================================*/
static bool Epot_IsValidAddress(uint8_t address)
{
    return (address >= EPOT_I2C_ADDRESS_MIN) && (address <= EPOT_I2C_ADDRESS_MAX);
}

static bool Epot_IsValidChannel(epot_channel_t channel)
{
    return ((int)channel >= 0) && (channel < EPOT_CHANNEL_COUNT);
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

static void Epot_ResetDevice(epot_device_t *device)
{
    if (device == NULL)
    {
        return;
    }

    device->handle = NULL;
    device->last_raw = 0U;
    device->has_last_raw = false;
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

    Epot_ResetDevice(&gDevices[EPOT_CHANNEL_GAIN]);
    Epot_ResetDevice(&gDevices[EPOT_CHANNEL_OFFSET]);

    err = Epot_AddDevice(bus_handle,
                         gain_address,
                         &gDevices[EPOT_CHANNEL_GAIN].handle);
    if (err != ESP_OK)
    {
        Epot_ResetDevice(&gDevices[EPOT_CHANNEL_GAIN]);
        return err;
    }

    err = Epot_AddDevice(bus_handle,
                         offset_address,
                         &gDevices[EPOT_CHANNEL_OFFSET].handle);
    if (err != ESP_OK)
    {
        i2c_master_bus_rm_device(gDevices[EPOT_CHANNEL_GAIN].handle);
        Epot_ResetDevice(&gDevices[EPOT_CHANNEL_GAIN]);
        Epot_ResetDevice(&gDevices[EPOT_CHANNEL_OFFSET]);
        return err;
    }

    gEpotInitialized = true;
    return ESP_OK;
}

esp_err_t Epot_Deinit(void)
{
    esp_err_t first_error = ESP_OK;

    if (!gEpotInitialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    for (int channel = 0; channel < EPOT_CHANNEL_COUNT; channel++)
    {
        epot_device_t *device = &gDevices[channel];

        if (device->handle != NULL)
        {
            const esp_err_t err = i2c_master_bus_rm_device(device->handle);
            if ((err != ESP_OK) && (first_error == ESP_OK))
            {
                first_error = err;
            }
        }

        Epot_ResetDevice(device);
    }

    gEpotInitialized = false;
    return first_error;
}

esp_err_t Epot_SetRaw(epot_channel_t channel, uint8_t value)
{
    esp_err_t err;
    epot_device_t *device;

    if (!gEpotInitialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (!Epot_IsValidChannel(channel))
    {
        return ESP_ERR_INVALID_ARG;
    }

    device = &gDevices[channel];
    if (device->handle == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    err = Epot_WriteWiper(device->handle, value);
    if (err != ESP_OK)
    {
        return err;
    }

    device->last_raw = value;
    device->has_last_raw = true;
    return ESP_OK;
}

esp_err_t Epot_SetGainRaw(uint8_t value)
{
    return Epot_SetRaw(EPOT_CHANNEL_GAIN, value);
}

esp_err_t Epot_SetOffsetRaw(uint8_t value)
{
    return Epot_SetRaw(EPOT_CHANNEL_OFFSET, value);
}

esp_err_t Epot_GetLastRaw(epot_channel_t channel, uint8_t *out_value)
{
    const epot_device_t *device;

    if (out_value == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!gEpotInitialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (!Epot_IsValidChannel(channel))
    {
        return ESP_ERR_INVALID_ARG;
    }

    device = &gDevices[channel];
    if ((device->handle == NULL) || !device->has_last_raw)
    {
        return ESP_ERR_INVALID_STATE;
    }

    *out_value = device->last_raw;
    return ESP_OK;
}

bool Epot_IsInitialized(void)
{
    return gEpotInitialized;
}

bool Epot_IsChannelAvailable(epot_channel_t channel)
{
    return gEpotInitialized &&
           Epot_IsValidChannel(channel) &&
           (gDevices[channel].handle != NULL);
}

/*===============================================================================================
FIN DU FICHIER
===============================================================================================*/

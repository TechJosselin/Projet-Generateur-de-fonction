/*===============================================================================================
CFPT - Projet : Générateur de fonction
Fichier      : initialisation.c
Description  : Initialisation des ressources matérielles communes du système
===============================================================================================*/

/*===============================================================================================
INCLUDES
===============================================================================================*/
#include "initialisation.h"
#include "hardware_config.h"

/*===============================================================================================
VARIABLES LOCALES
===============================================================================================*/
static i2c_master_bus_handle_t gI2cBusHandle = NULL;
static bool gSystemInitialized = false;

/*===============================================================================================
PROTOTYPES DE FONCTIONS LOCALES
===============================================================================================*/
static esp_err_t InitI2cBus(void);

/*===============================================================================================
FONCTIONS LOCALES
===============================================================================================*/
static esp_err_t InitI2cBus(void)
{
    if (gI2cBusHandle != NULL)
    {
        return ESP_OK;
    }

    const i2c_master_bus_config_t bus_config =
    {
        .i2c_port = SYSTEM_I2C_PORT,
        .sda_io_num = SYSTEM_I2C_SDA_GPIO,
        .scl_io_num = SYSTEM_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = SYSTEM_I2C_GLITCH_IGNORE_COUNT,
        .flags.enable_internal_pullup = SYSTEM_I2C_ENABLE_INTERNAL_PULLUPS,
    };

    return i2c_new_master_bus(&bus_config, &gI2cBusHandle);
}

/*===============================================================================================
FONCTIONS PUBLIQUES
===============================================================================================*/
esp_err_t System_Init(void)
{
    esp_err_t err;

    if (gSystemInitialized)
    {
        return ESP_OK;
    }

    err = InitI2cBus();
    if (err != ESP_OK)
    {
        return err;
    }

    gSystemInitialized = true;
    return ESP_OK;
}

esp_err_t System_Deinit(void)
{
    esp_err_t err;

    if (!gSystemInitialized)
    {
        return ESP_OK;
    }

    if (gI2cBusHandle != NULL)
    {
        err = i2c_del_master_bus(gI2cBusHandle);
        if (err != ESP_OK)
        {
            return err;
        }

        gI2cBusHandle = NULL;
    }

    gSystemInitialized = false;
    return ESP_OK;
}

i2c_master_bus_handle_t System_GetI2cBus(void)
{
    return gI2cBusHandle;
}

bool System_IsInitialized(void)
{
    return gSystemInitialized;
}

/*===============================================================================================
FIN DU FICHIER
===============================================================================================*/

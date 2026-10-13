/*===============================================================================================
CFPT - Projet : Générateur de fonction
Fichier      : initialisation.c
Description  : Initialisation des ressources matérielles communes du système
===============================================================================================*/

/*===============================================================================================
INCLUDES
===============================================================================================*/
#include <stdio.h>

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
static const char *I2cAddressDescription(uint8_t address);

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

static const char *I2cAddressDescription(uint8_t address)
{
    switch (address)
    {
        case SYSTEM_OLED_I2C_ADDRESS_PRIMARY:
            return "OLED SH1106 (adresse primaire attendue)";

        case SYSTEM_OLED_I2C_ADDRESS_SECONDARY:
            return "OLED SH1106 (adresse secondaire possible)";

        case SYSTEM_EPOT_OFFSET_I2C_ADDRESS:
            return "EPOT Offset MCP45HV51";

        case SYSTEM_EPOT_GAIN_I2C_ADDRESS:
            return "EPOT Gain MCP45HV51";

        case 0x50U:
            return "EEPROM 24LC32 possible (A2=A1=A0=0)";

        case 0x51U:
        case 0x52U:
        case 0x53U:
        case 0x54U:
        case 0x55U:
        case 0x56U:
        case 0x57U:
            return "EEPROM 24LC32 possible";

        default:
            return "peripherique I2C non identifie";
    }
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

    /* Premier diagnostic immédiatement après la création du bus. */
    System_ScanI2cBus();

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

void System_ScanI2cBus(void)
{
    uint16_t detected_count = 0U;

    if (gI2cBusHandle == NULL)
    {
        printf("\n===== SCAN I2C =====\n");
        printf("Impossible : bus I2C non initialise.\n");
        printf("====================\n\n");
        fflush(stdout);
        return;
    }

    printf("\n===== SCAN I2C COMPLET =====\n");
    printf("SDA = GPIO%d / SCL = GPIO%d\n",
           (int)SYSTEM_I2C_SDA_GPIO,
           (int)SYSTEM_I2C_SCL_GPIO);
    printf("Recherche des adresses 7 bits 0x08..0x77...\n");

    for (uint8_t address = 0x08U; address <= 0x77U; address++)
    {
        const esp_err_t err = i2c_master_probe(gI2cBusHandle, address, 10);

        if (err == ESP_OK)
        {
            detected_count++;
            printf("  0x%02X : DETECTE -> %s\n",
                   address,
                   I2cAddressDescription(address));
        }
    }

    if (detected_count == 0U)
    {
        printf("Aucun peripherique I2C detecte.\n");
    }
    else
    {
        printf("Total : %u peripherique(s) detecte(s).\n", detected_count);
    }

    printf("Attendu actuellement : OLED=0x3C/0x3D, Offset=0x3E, Gain=0x3F, EEPROM=0x50 si A0/A1/A2=0.\n");
    printf("=============================\n\n");
    fflush(stdout);
}

bool System_IsInitialized(void)
{
    return gSystemInitialized;
}

/*===============================================================================================
FIN DU FICHIER
===============================================================================================*/

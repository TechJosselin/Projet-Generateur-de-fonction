/*===============================================================================================
CFPT - Projet : Generateur de fonction
Fichier      : initialisation.c
Description  : Compatibilite avec l'ancienne API d'initialisation.
               Le proprietaire unique du bus I2C est maintenant i2c_commun.c.
===============================================================================================*/
#include <stdio.h>

#include "initialisation.h"
#include "hardware_config.h"
#include "i2c_commun.h"

static bool gSystemInitialized = false;

static const char *I2cAddressDescription(uint8_t address)
{
    switch (address)
    {
        case SYSTEM_OLED_I2C_ADDRESS_PRIMARY:   return "OLED SH1106 (primaire)";
        case SYSTEM_OLED_I2C_ADDRESS_SECONDARY: return "OLED SH1106 (secondaire)";
        case SYSTEM_EPOT_OFFSET_I2C_ADDRESS:    return "EPOT Offset MCP45HV51 (A1=1 A0=0)";
        case SYSTEM_EPOT_GAIN_I2C_ADDRESS:      return "EPOT Gain MCP45HV51 (A1=1 A0=1)";
        case SYSTEM_EEPROM_I2C_ADDRESS:         return "EEPROM 24LC32 (A2=A1=A0=0)";
        default:
            if ((address >= SYSTEM_EEPROM_I2C_ADDRESS_MIN) &&
                (address <= SYSTEM_EEPROM_I2C_ADDRESS_MAX))
            {
                return "EEPROM 24LC32 autre configuration d'adresse";
            }
            return "peripherique I2C non identifie";
    }
}

esp_err_t System_Init(void)
{
    const esp_err_t err = i2c_commun_initialiser();
    if (err == ESP_OK)
    {
        gSystemInitialized = true;
    }
    return err;
}

esp_err_t System_Deinit(void)
{
    const esp_err_t err = i2c_commun_desinitialiser();
    if (err == ESP_OK)
    {
        gSystemInitialized = false;
    }
    return err;
}

i2c_master_bus_handle_t System_GetI2cBus(void)
{
    return i2c_commun_bus();
}

void System_ScanI2cBus(void)
{
    const i2c_master_bus_handle_t bus = i2c_commun_bus();
    if (bus == NULL)
    {
        printf("I2C: bus non initialise.\n");
        return;
    }

    unsigned trouves = 0U;

    printf("\n===== SCAN I2C BRUT =====\n");
    for (uint8_t address = 0x08U; address <= 0x77U; address++)
    {
        const esp_err_t err = i2c_master_probe(bus,
                                               address,
                                               SYSTEM_I2C_DIAG_PROBE_TIMEOUT_MS);
        if (err == ESP_OK)
        {
            ++trouves;
            printf("0x%02X : %s\n", address, I2cAddressDescription(address));
        }
    }
    printf("Total detecte : %u peripherique(s)\n", trouves);
    printf("Attendu       : 0x3C OLED, 0x3E OFFSET, 0x3F GAIN, 0x50 EEPROM\n");
    printf("=========================\n\n");
}

bool System_IsInitialized(void)
{
    return gSystemInitialized && (i2c_commun_bus() != NULL);
}

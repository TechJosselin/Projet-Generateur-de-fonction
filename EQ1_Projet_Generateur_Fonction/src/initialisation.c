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
    if (address == SYSTEM_OLED_I2C_ADDRESS_PRIMARY)
    {
        return "OLED SH1106 (primaire)";
    }
    if (address == SYSTEM_OLED_I2C_ADDRESS_SECONDARY)
    {
        return "OLED SH1106 (secondaire)";
    }
    if (address == SYSTEM_EPOT_OFFSET_I2C_ADDRESS)
    {
        return "EPOT Offset MCP45HV51";
    }
    if (address == SYSTEM_EPOT_GAIN_I2C_ADDRESS)
    {
        return "EPOT Gain MCP45HV51";
    }
    if ((address >= SYSTEM_EEPROM_I2C_ADDRESS_MIN) &&
        (address <= SYSTEM_EEPROM_I2C_ADDRESS_MAX))
    {
        return "EEPROM 24LC32 possible";
    }
    return "peripherique I2C non identifie";
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
    if (!i2c_commun_est_initialise())
    {
        printf("I2C: bus non initialise.\n");
        return;
    }

    printf("\n===== SCAN I2C =====\n");
    for (uint8_t address = 0x08U; address <= 0x77U; address++)
    {
        if (i2c_commun_adresse_presente(address, SYSTEM_I2C_DIAG_PROBE_TIMEOUT_MS))
        {
            printf("0x%02X : %s\n", address, I2cAddressDescription(address));
        }
    }
    printf("====================\n");
}

bool System_IsInitialized(void)
{
    return gSystemInitialized && i2c_commun_est_initialise();
}

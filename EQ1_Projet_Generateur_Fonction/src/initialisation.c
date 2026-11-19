/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : INITIALISATION.C
 * DESCRIPTIF : Demarrage des sous-systemes de l'ESP32-C6.
 *==================================================================================================*/
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

/*============================== VARIABLES ET CONFIGURATION =======================================*/
/* Adresses 7 bits accessibles par balayage sur le bus I2C. */
#define I2C_SCAN_ADRESSE_MIN 0x08U
#define I2C_SCAN_ADRESSE_MAX 0x77U
static bool systeme_initialise = false;
/*==================================== FONCTIONS ================================================*/
static const char *I2cAddressDescription(uint8_t adresse_i2c)
{
    switch (adresse_i2c)
    {
    case SYSTEM_OLED_I2C_ADDRESS_PRIMARY:
        return "OLED SH1106 (primaire)";
    case SYSTEM_OLED_I2C_ADDRESS_SECONDARY:
        return "OLED SH1106 (secondaire)";
    case SYSTEM_EPOT_OFFSET_I2C_ADDRESS:
        return "EPOT Offset MCP45HV51 (A1=1 A0=0)";
    case SYSTEM_EPOT_GAIN_I2C_ADDRESS:
        return "EPOT Gain MCP45HV51 (A1=1 A0=1)";
    case SYSTEM_EEPROM_I2C_ADDRESS:
        return "EEPROM 24LC32 (A2=A1=A0=0)";
    default:
        if ((adresse_i2c >= SYSTEM_EEPROM_I2C_ADDRESS_MIN) &&(adresse_i2c <= SYSTEM_EEPROM_I2C_ADDRESS_MAX))
        {
            return "EEPROM 24LC32 autre configuration d'adresse";
        }
        return "peripherique I2C non identifie";
    }
}

/*================================ INTERFACE PUBLIQUE ============================================*/
esp_err_t System_Init(void)
{
    const esp_err_t resultat_i2c = i2c_commun_initialiser();
    if (resultat_i2c == ESP_OK)
    {
        systeme_initialise = true;
    }
    return resultat_i2c;
}

esp_err_t System_Deinit(void)
{
    const esp_err_t resultat_i2c = i2c_commun_desinitialiser();
    if (resultat_i2c == ESP_OK)
    {
        systeme_initialise = false;
    }
    return resultat_i2c;
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
    unsigned nombre_detectes = 0U;
    printf("\n===== SCAN I2C BRUT =====\n");
    for (uint8_t adresse_i2c = I2C_SCAN_ADRESSE_MIN; adresse_i2c <= I2C_SCAN_ADRESSE_MAX; adresse_i2c++)
    {
        const esp_err_t resultat_i2c = i2c_master_probe(bus, adresse_i2c, SYSTEM_I2C_DIAG_PROBE_TIMEOUT_MS);
        if (resultat_i2c == ESP_OK)
        {
            ++ nombre_detectes;
            printf("0x%02X : %s\n", adresse_i2c, I2cAddressDescription(adresse_i2c));
        }
    }
    printf("Total detecte : %u peripherique(s)\n", nombre_detectes);
    printf("Attendu       : 0x3C OLED, 0x3E OFFSET, 0x3F GAIN, 0x50 EEPROM\n");
    printf("=========================\n\n");
}

bool System_IsInitialized(void)
{
    return systeme_initialise &&(i2c_commun_bus() != NULL);
}

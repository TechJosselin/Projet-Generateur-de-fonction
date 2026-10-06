/*===============================================================================================
CFPT - Projet : Générateur de fonction
Fichier      : main.c
Description  : Point d'entrée principal du programme
===============================================================================================*/

/*===============================================================================================
INCLUDES
===============================================================================================*/
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c_master.h"
#include "esp_err.h"

#include "initialisation.h"
#include "ad9833.h"
#include "oled.h"
#include "eeprom.h"
#include "epot.h"
#include "encodeur.h"
#include "usb_protocol.h"
#include "signaux.h"
#include "sorties_digitales.h"

/*===============================================================================================
DEFINES
===============================================================================================*/
/* Bus I2C du XIAO ESP32-C6 utilisé par OLED / EEPROM / EPOT. */
#define I2C_SDA_GPIO                22
#define I2C_SCL_GPIO                23
#define I2C_PORT                    I2C_NUM_0

/*
 * MCP45HV51 : adresse I2C = 0b01111 A1 A0.
 * Pour ce test :
 *   Gain   -> A1 = 1, A0 = 0 -> 0x3E
 *   Offset -> A1 = 1, A0 = 1 -> 0x3F
 *
 * Les adresses 0x3C et 0x3D restent ainsi disponibles pour l'écran OLED.
 */
#define EPOT_GAIN_I2C_ADDRESS       0x3EU
#define EPOT_OFFSET_I2C_ADDRESS     0x3FU

#define EPOT_TEST_GAIN_RAW          127U
#define EPOT_TEST_OFFSET_RAW        127U

/*===============================================================================================
VARIABLES GLOBALES
===============================================================================================*/
static i2c_master_bus_handle_t gI2cBusHandle = NULL;

/*===============================================================================================
PROTOTYPES DE FONCTIONS LOCALES
===============================================================================================*/
static esp_err_t InitI2cBusForTest(void);
static esp_err_t TestAnalogChainEpots(void);

/*===============================================================================================
FONCTIONS LOCALES
===============================================================================================*/
static esp_err_t InitI2cBusForTest(void)
{
    if (gI2cBusHandle != NULL)
    {
        return ESP_OK;
    }

    const i2c_master_bus_config_t bus_config =
    {
        .i2c_port = I2C_PORT,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    return i2c_new_master_bus(&bus_config, &gI2cBusHandle);
}

static esp_err_t TestAnalogChainEpots(void)
{
    esp_err_t err;

    err = InitI2cBusForTest();
    if (err != ESP_OK)
    {
        printf("Erreur initialisation bus I2C : %s\n", esp_err_to_name(err));
        return err;
    }

    err = Epot_Init(gI2cBusHandle,
                    EPOT_GAIN_I2C_ADDRESS,
                    EPOT_OFFSET_I2C_ADDRESS);
    if (err != ESP_OK)
    {
        printf("Erreur initialisation EPOT : %s\n", esp_err_to_name(err));
        return err;
    }

    err = Epot_SetGainRaw(EPOT_TEST_GAIN_RAW);
    if (err != ESP_OK)
    {
        printf("Erreur ecriture EPOT Gain : %s\n", esp_err_to_name(err));
        return err;
    }

    err = Epot_SetOffsetRaw(EPOT_TEST_OFFSET_RAW);
    if (err != ESP_OK)
    {
        printf("Erreur ecriture EPOT Offset : %s\n", esp_err_to_name(err));
        return err;
    }

    printf("Test EPOT OK : Gain=%u / Offset=%u\n",
           EPOT_TEST_GAIN_RAW,
           EPOT_TEST_OFFSET_RAW);

    return ESP_OK;
}

/*===============================================================================================
MAIN
===============================================================================================*/
void app_main(void)
{
    esp_err_t err = TestAnalogChainEpots();

    if (err != ESP_OK)
    {
        printf("Test chaine analogique EPOT echoue.\n");
        return;
    }

    printf("Driver MCP45HV51 initialise. Verifier VOUT a l'oscilloscope.\n");
}

/*===============================================================================================
FIN DU MAIN
===============================================================================================*/

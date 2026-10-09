/*===============================================================================================
CFPT - Projet : Générateur de fonction
Fichier      : main.c
Description  : Test d'un seul MCP45HV51 à l'adresse 0x3F
===============================================================================================*/

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"

#include "initialisation.h"
#include "epot.h"

#define EPOT_SINGLE_TEST_I2C_ADDRESS      0x3FU
#define EPOT_SINGLE_TEST_DELAY_MS         3000U

static const uint8_t gTestValues[] =
{
    0U,
    64U,
    128U,
    192U,
    255U
};

static void FatalLoop(const char *message, esp_err_t err)
{
    printf("ERREUR : %s : %s\n", message, esp_err_to_name(err));
    fflush(stdout);

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    esp_err_t err;

    printf("\n========================================\n");
    printf("TEST MCP45HV51 - UN SEUL EPOT\n");
    printf("Adresse I2C : 0x%02X\n", EPOT_SINGLE_TEST_I2C_ADDRESS);
    printf("Valeurs testees : 0, 64, 128, 192, 255\n");
    printf("Changement toutes les %u ms\n", EPOT_SINGLE_TEST_DELAY_MS);
    printf("========================================\n\n");
    fflush(stdout);

    err = System_Init();
    if (err != ESP_OK)
    {
        FatalLoop("System_Init", err);
    }

    printf("Bus I2C initialise.\n");

    err = Epot_InitSingle(System_GetI2cBus(), EPOT_SINGLE_TEST_I2C_ADDRESS);
    if (err != ESP_OK)
    {
        FatalLoop("Epot_InitSingle", err);
    }

    printf("Driver MCP45HV51 initialise en mode simple.\n");
    printf("Mesurer PW0 par rapport a DGND pendant que les valeurs changent.\n\n");
    fflush(stdout);

    while (1)
    {
        for (size_t i = 0; i < (sizeof(gTestValues) / sizeof(gTestValues[0])); i++)
        {
            const uint8_t value = gTestValues[i];

            err = Epot_SetSingleRaw(value);

            if (err == ESP_OK)
            {
                printf("MCP45HV51 OK - Wiper = %3u / 255\n", value);
            }
            else
            {
                printf("MCP45HV51 ECHEC - Wiper = %3u : %s\n",
                       value,
                       esp_err_to_name(err));
            }

            fflush(stdout);
            vTaskDelay(pdMS_TO_TICKS(EPOT_SINGLE_TEST_DELAY_MS));
        }
    }
}

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

#include "esp_err.h"
#include "esp_system.h"
#include "driver/gpio.h"

#include "hardware_config.h"
#include "initialisation.h"

/*===============================================================================================
PROTOTYPES DE FONCTIONS LOCALES
===============================================================================================*/
static esp_err_t InitStatusLed(void);
static void StatusLedSet(int enabled);
static const char *ResetReasonToString(esp_reset_reason_t reason);
static void FatalBlinkLoop(const char *message, esp_err_t error);

/*===============================================================================================
FONCTIONS LOCALES
===============================================================================================*/
static esp_err_t InitStatusLed(void)
{
    const gpio_config_t led_config =
    {
        .pin_bit_mask = (1ULL << SYSTEM_STATUS_LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&led_config);
    if (err != ESP_OK)
    {
        return err;
    }

    return gpio_set_level(SYSTEM_STATUS_LED_GPIO, SYSTEM_STATUS_LED_OFF_LEVEL);
}

static void StatusLedSet(int enabled)
{
    gpio_set_level(
        SYSTEM_STATUS_LED_GPIO,
        enabled ? SYSTEM_STATUS_LED_ON_LEVEL : SYSTEM_STATUS_LED_OFF_LEVEL);
}

static const char *ResetReasonToString(esp_reset_reason_t reason)
{
    switch (reason)
    {
        case ESP_RST_UNKNOWN:   return "UNKNOWN";
        case ESP_RST_POWERON:   return "POWERON";
        case ESP_RST_EXT:       return "EXT";
        case ESP_RST_SW:        return "SW";
        case ESP_RST_PANIC:     return "PANIC";
        case ESP_RST_INT_WDT:   return "INT_WDT";
        case ESP_RST_TASK_WDT:  return "TASK_WDT";
        case ESP_RST_WDT:       return "WDT";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
        case ESP_RST_BROWNOUT:  return "BROWNOUT";
        case ESP_RST_SDIO:      return "SDIO";
        case ESP_RST_USB:       return "USB";
        case ESP_RST_JTAG:      return "JTAG";
        case ESP_RST_EFUSE:     return "EFUSE";
        case ESP_RST_PWR_GLITCH:return "PWR_GLITCH";
        case ESP_RST_CPU_LOCKUP:return "CPU_LOCKUP";
        default:                return "OTHER";
    }
}

static void FatalBlinkLoop(const char *message, esp_err_t error)
{
    printf("ERREUR FATALE : %s : %s\n", message, esp_err_to_name(error));
    fflush(stdout);

    while (1)
    {
        StatusLedSet(1);
        vTaskDelay(pdMS_TO_TICKS(100));
        StatusLedSet(0);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/*===============================================================================================
MAIN
===============================================================================================*/
void app_main(void)
{
    esp_err_t err;
    esp_reset_reason_t reset_reason;
    unsigned int heartbeat_count = 0;

    reset_reason = esp_reset_reason();

    err = InitStatusLed();
    if (err != ESP_OK)
    {
        printf("Erreur initialisation LED : %s\n", esp_err_to_name(err));
        fflush(stdout);
    }

    printf("\n========================================\n");
    printf("DEMARRAGE GENERATEUR DE FONCTION\n");
    printf("Reset reason : %d (%s)\n", (int)reset_reason, ResetReasonToString(reset_reason));
    printf("========================================\n");
    fflush(stdout);

    /* Impulsion visuelle de démarrage. */
    StatusLedSet(1);
    vTaskDelay(pdMS_TO_TICKS(250));
    StatusLedSet(0);
    vTaskDelay(pdMS_TO_TICKS(250));

    printf("Initialisation systeme...\n");
    fflush(stdout);

    err = System_Init();
    if (err != ESP_OK)
    {
        FatalBlinkLoop("System_Init", err);
    }

    printf("Initialisation systeme OK.\n");
    printf("Bus I2C partage pret pour OLED, EEPROM et EPOT.\n");
    printf("Heartbeat actif : LED GPIO15, periode 1 seconde.\n");
    fflush(stdout);

    /*
     * Boucle de diagnostic temporaire :
     * - la LED change d'etat toutes les 500 ms ;
     * - vTaskDelay() laisse le CPU aux autres taches et au watchdog ;
     * - un message periodique permet de voir la console meme si le moniteur
     *   serie est ouvert apres le demarrage de la carte.
     */
    while (1)
    {
        StatusLedSet(1);
        vTaskDelay(pdMS_TO_TICKS(500));

        StatusLedSet(0);
        vTaskDelay(pdMS_TO_TICKS(500));

        heartbeat_count++;
        if ((heartbeat_count % 2U) == 0U)
        {
            printf("Heartbeat OK - reset reason : %s\n", ResetReasonToString(reset_reason));
            fflush(stdout);
        }
    }
}

/*===============================================================================================
FIN DU MAIN
===============================================================================================*/

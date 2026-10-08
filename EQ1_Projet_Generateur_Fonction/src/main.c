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
#include "esp_timer.h"
#include "driver/gpio.h"

#include "hardware_config.h"
#include "initialisation.h"
#include "epot.h"
#include "oled.h"

/*===============================================================================================
VARIABLES LOCALES
===============================================================================================*/
static bool gEpotCommunicationOk = false;
static bool gOledCommunicationOk = false;

/*===============================================================================================
PROTOTYPES DE FONCTIONS LOCALES
===============================================================================================*/
static esp_err_t InitStatusLed(void);
static void StatusLedSet(int enabled);
static const char *ResetReasonToString(esp_reset_reason_t reason);
static void PrintSystemStatus(esp_reset_reason_t reset_reason, unsigned long heartbeat_count);
static void FatalBlinkLoop(const char *message, esp_err_t error);
static void TestEpotCommunication(void);
static void InitAndTestOled(void);

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
        case ESP_RST_UNKNOWN:    return "UNKNOWN";
        case ESP_RST_POWERON:    return "POWERON";
        case ESP_RST_EXT:        return "EXT";
        case ESP_RST_SW:         return "SW";
        case ESP_RST_PANIC:      return "PANIC";
        case ESP_RST_INT_WDT:    return "INT_WDT";
        case ESP_RST_TASK_WDT:   return "TASK_WDT";
        case ESP_RST_WDT:        return "WDT";
        case ESP_RST_DEEPSLEEP:  return "DEEPSLEEP";
        case ESP_RST_BROWNOUT:   return "BROWNOUT";
        case ESP_RST_SDIO:       return "SDIO";
        case ESP_RST_USB:        return "USB";
        case ESP_RST_JTAG:       return "JTAG";
        case ESP_RST_EFUSE:      return "EFUSE";
        case ESP_RST_PWR_GLITCH: return "PWR_GLITCH";
        case ESP_RST_CPU_LOCKUP: return "CPU_LOCKUP";
        default:                 return "OTHER";
    }
}

static void PrintSystemStatus(esp_reset_reason_t reset_reason, unsigned long heartbeat_count)
{
    const bool system_initialized = System_IsInitialized();
    const i2c_master_bus_handle_t i2c_bus = System_GetI2cBus();
    const bool epot_initialized = Epot_IsInitialized();
    const bool oled_initialized = Oled_IsInitialized();

    printf("\n========================================\n");
    printf("ETAT SYSTEME\n");
    printf("Heartbeat     : #%lu\n", heartbeat_count);
    printf("Uptime        : %lld ms\n", (long long)(esp_timer_get_time() / 1000LL));
    printf("Reset reason  : %s\n", ResetReasonToString(reset_reason));
    printf("System_Init   : %s\n", system_initialized ? "OK" : "NON INITIALISE");
    printf("Bus I2C       : %s\n", (i2c_bus != NULL) ? "READY" : "NULL");
    printf("Handle I2C    : %p\n", (void *)i2c_bus);
    printf("Driver OLED   : %s\n", oled_initialized ? "INITIALISE" : "NON INITIALISE");
    printf("OLED adresse  : %s", oled_initialized ? "0x" : "--");
    if (oled_initialized)
    {
        printf("%02X", Oled_GetI2cAddress());
    }
    printf("\n");
    printf("Test OLED     : %s\n", gOledCommunicationOk ? "OK" : "ECHEC / NON TESTE");
    printf("Driver EPOT   : %s\n", epot_initialized ? "INITIALISE" : "NON INITIALISE");
    printf("EPOT Gain     : adresse 0x%02X\n", SYSTEM_EPOT_GAIN_I2C_ADDRESS);
    printf("EPOT Offset   : adresse 0x%02X\n", SYSTEM_EPOT_OFFSET_I2C_ADDRESS);
    printf("Test EPOT I2C : %s\n", gEpotCommunicationOk ? "OK" : "ECHEC / NON TESTE");
    printf("========================================\n\n");
    fflush(stdout);
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

static void TestEpotCommunication(void)
{
    esp_err_t gain_err;
    esp_err_t offset_err;

    gain_err = Epot_SetGainRaw(SYSTEM_EPOT_TEST_RAW_VALUE);
    offset_err = Epot_SetOffsetRaw(SYSTEM_EPOT_TEST_RAW_VALUE);

    if ((gain_err == ESP_OK) && (offset_err == ESP_OK))
    {
        gEpotCommunicationOk = true;
        printf("Test EPOT I2C OK : Gain=%u, Offset=%u\n",
               SYSTEM_EPOT_TEST_RAW_VALUE,
               SYSTEM_EPOT_TEST_RAW_VALUE);
    }
    else
    {
        gEpotCommunicationOk = false;
        printf("Test EPOT I2C ECHEC.\n");
        printf("  Gain   : %s\n", esp_err_to_name(gain_err));
        printf("  Offset : %s\n", esp_err_to_name(offset_err));
    }

    fflush(stdout);
}

static void InitAndTestOled(void)
{
    esp_err_t err;

    printf("Initialisation OLED SH1106...\n");

    err = Oled_Init(System_GetI2cBus());
    if (err != ESP_OK)
    {
        gOledCommunicationOk = false;
        printf("OLED non detecte sur 0x%02X ou 0x%02X : %s\n",
               SYSTEM_OLED_I2C_ADDRESS_PRIMARY,
               SYSTEM_OLED_I2C_ADDRESS_SECONDARY,
               esp_err_to_name(err));
        fflush(stdout);
        return;
    }

    printf("OLED detecte a l'adresse 0x%02X.\n", Oled_GetI2cAddress());

    err = Oled_TestPattern();
    if (err == ESP_OK)
    {
        gOledCommunicationOk = true;
        printf("Test OLED OK : cadre et croix affiches.\n");
    }
    else
    {
        gOledCommunicationOk = false;
        printf("Test OLED ECHEC : %s\n", esp_err_to_name(err));
    }

    fflush(stdout);
}

/*===============================================================================================
MAIN
===============================================================================================*/
void app_main(void)
{
    esp_err_t err;
    esp_reset_reason_t reset_reason;
    unsigned long heartbeat_count = 0;

    reset_reason = esp_reset_reason();

    err = InitStatusLed();
    if (err != ESP_OK)
    {
        printf("Erreur initialisation LED : %s\n", esp_err_to_name(err));
        fflush(stdout);
    }

    StatusLedSet(1);
    vTaskDelay(pdMS_TO_TICKS(3000));
    StatusLedSet(0);

    printf("\n========================================\n");
    printf("DEMARRAGE GENERATEUR DE FONCTION\n");
    printf("Reset reason : %d (%s)\n", (int)reset_reason, ResetReasonToString(reset_reason));
    printf("Uptime au debut du diagnostic : %lld ms\n", (long long)(esp_timer_get_time() / 1000LL));
    printf("========================================\n");
    fflush(stdout);

    printf("Initialisation systeme...\n");
    err = System_Init();
    if (err != ESP_OK)
    {
        FatalBlinkLoop("System_Init", err);
    }

    printf("Initialisation systeme OK.\n");
    printf("Bus I2C partage pret.\n");

    InitAndTestOled();

    printf("Initialisation EPOT...\n");
    err = Epot_Init(System_GetI2cBus(),
                    SYSTEM_EPOT_GAIN_I2C_ADDRESS,
                    SYSTEM_EPOT_OFFSET_I2C_ADDRESS);
    if (err != ESP_OK)
    {
        FatalBlinkLoop("Epot_Init", err);
    }

    printf("Driver EPOT initialise.\n");
    TestEpotCommunication();
    printf("Diagnostic heartbeat actif.\n");
    fflush(stdout);

    while (1)
    {
        const bool system_initialized = System_IsInitialized();
        const i2c_master_bus_handle_t i2c_bus = System_GetI2cBus();
        const bool epot_initialized = Epot_IsInitialized();
        const bool oled_initialized = Oled_IsInitialized();

        StatusLedSet(1);
        vTaskDelay(pdMS_TO_TICKS(500));

        StatusLedSet(0);
        vTaskDelay(pdMS_TO_TICKS(500));

        heartbeat_count++;

        printf(
            "Heartbeat #%lu - uptime : %lld ms - System : %s - I2C : %s - OLED : %s/%s - EPOT : %s/%s\n",
            heartbeat_count,
            (long long)(esp_timer_get_time() / 1000LL),
            system_initialized ? "OK" : "ERREUR",
            (i2c_bus != NULL) ? "READY" : "NULL",
            oled_initialized ? "INIT" : "ABSENT",
            gOledCommunicationOk ? "OK" : "ECHEC",
            epot_initialized ? "INIT" : "ERREUR",
            gEpotCommunicationOk ? "OK" : "ECHEC");
        fflush(stdout);

        if ((heartbeat_count % 5UL) == 0UL)
        {
            PrintSystemStatus(reset_reason, heartbeat_count);
        }
    }
}

/*===============================================================================================
FIN DU MAIN
===============================================================================================*/

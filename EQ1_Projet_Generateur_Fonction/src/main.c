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
#include "encodeur.h"
#include "epot.h"
#include "oled.h"
#include "ad9833.h"
#include "signaux.h"

/*===============================================================================================
CONSTANTES LOCALES
===============================================================================================*/
#define ENCODER_BUTTON_DEBOUNCE_MS              30U
#define ENCODER_BUTTON_DEBOUNCE_US              ((int64_t)ENCODER_BUTTON_DEBOUNCE_MS * 1000LL)

/*
 * Mode d'intégration actuel :
 *   Encodeur -> valeur 0..255 -> OLED
 *                            -> fréquence AD9833 en Hz
 *                            -> wiper brut MCP45HV51 Gain
 *                            -> wiper brut MCP45HV51 Offset
 *
 * Ce couplage permet de vérifier toute la chaîne logicielle en temps réel.
 * Il sera remplacé ensuite par les vraies conversions fréquence / amplitude / offset.
 */
#define LIVE_CONTROL_MIN_VALUE                  ((int32_t)EPOT_RAW_MIN)
#define LIVE_CONTROL_MAX_VALUE                  ((int32_t)EPOT_RAW_MAX)
#define LIVE_CONTROL_INITIAL_VALUE              127

/*===============================================================================================
VARIABLES LOCALES
===============================================================================================*/
static bool gOledCommunicationOk = false;
static bool gAd9833CommandOk = false;
static bool gEpotCommunicationOk = false;

/*===============================================================================================
PROTOTYPES DE FONCTIONS LOCALES
===============================================================================================*/
static esp_err_t InitStatusLed(void);
static void StatusLedSet(int enabled);
static const char *ResetReasonToString(esp_reset_reason_t reason);
static void PrintSystemStatus(esp_reset_reason_t reset_reason, unsigned long heartbeat_count);
static void FatalBlinkLoop(const char *message, esp_err_t error);
static void InitAndTestOled(void);
static void InitEncoder(void);
static void InitEpot(void);
static void InitAndTestAd9833(void);
static int32_t ClampLiveControlValue(int32_t value);
static esp_err_t ApplyLiveControlValue(int32_t value);

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
    const bool encoder_initialized = Encodeur_IsInitialized();
    const bool ad9833_initialized = Ad9833_IsInitialized();
    const int32_t encoder_counter = Encodeur_GetCounter();
    signaux_signal_t signal;
    uint8_t gain_raw;
    uint8_t offset_raw;
    const bool signal_available = (Signaux_GetCurrent(&signal) == ESP_OK);
    const bool gain_raw_available = (Signaux_GetGainRaw(&gain_raw) == ESP_OK);
    const bool offset_raw_available = (Signaux_GetOffsetRaw(&offset_raw) == ESP_OK);

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
    printf("Encodeur      : %s\n", encoder_initialized ? "INITIALISE" : "NON INITIALISE");
    printf("Valeur live   : %ld\n", (long)encoder_counter);
    printf("Bouton SW     : %s\n", Encodeur_IsButtonPressed() ? "APPUYE" : "RELACHE");
    printf("Driver AD9833 : %s\n", ad9833_initialized ? "INITIALISE" : "NON INITIALISE");
    printf("Commande SPI  : %s\n", gAd9833CommandOk ? "OK" : "ECHEC / NON TESTEE");

    if (signal_available)
    {
        printf("Signal        : %s / %.3f Hz\n",
               Signaux_TypeToString(signal.type),
               (double)signal.frequency_hz);
        printf("Amplitude     : %.3f Vpp (consigne non calibree)\n",
               (double)signal.amplitude_vpp);
        printf("Offset        : %.3f V (consigne non calibree)\n",
               (double)signal.offset_v);
    }
    else
    {
        printf("Signal        : NON APPLIQUE\n");
    }

    printf("Driver EPOT   : %s / commande %s\n",
           epot_initialized ? "INITIALISE" : "NON INITIALISE",
           gEpotCommunicationOk ? "OK" : "NON VALIDEE");

    printf("EPOT Gain     : adresse 0x%02X / wiper ", SYSTEM_EPOT_GAIN_I2C_ADDRESS);
    if (gain_raw_available)
    {
        printf("%u\n", gain_raw);
    }
    else
    {
        printf("non commande\n");
    }

    printf("EPOT Offset   : adresse 0x%02X / wiper ", SYSTEM_EPOT_OFFSET_I2C_ADDRESS);
    if (offset_raw_available)
    {
        printf("%u\n", offset_raw);
    }
    else
    {
        printf("non commande\n");
    }

    printf("========================================\n");
    fflush(stdout);

    /* Le scan est répété à chaque affichage de l'état système pour faciliter le diagnostic. */
    System_ScanI2cBus();
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

    err = Oled_ShowCounter(0);
    if (err == ESP_OK)
    {
        gOledCommunicationOk = true;
        printf("OLED OK.\n");
    }
    else
    {
        gOledCommunicationOk = false;
        printf("Affichage OLED ECHEC : %s\n", esp_err_to_name(err));
    }

    fflush(stdout);
}

static void InitEncoder(void)
{
    const esp_err_t err = Encodeur_Init(
        SYSTEM_ENCODER_A_GPIO,
        SYSTEM_ENCODER_B_GPIO,
        SYSTEM_ENCODER_BUTTON_GPIO,
        SYSTEM_ENCODER_EDGES_PER_STEP,
        SYSTEM_ENCODER_REVERSE_DIRECTION != 0);

    if (err != ESP_OK)
    {
        FatalBlinkLoop("Encodeur_Init", err);
    }

    printf("Encodeur PEC12R initialise sous interruption.\n");
    printf("  A/CLK : GPIO%d\n", (int)SYSTEM_ENCODER_A_GPIO);
    printf("  B/DT  : GPIO%d\n", (int)SYSTEM_ENCODER_B_GPIO);
    printf("  SW    : GPIO%d\n", (int)SYSTEM_ENCODER_BUTTON_GPIO);
    fflush(stdout);
}

static void InitEpot(void)
{
    printf("Initialisation MCP45HV51...\n");
    printf("  Gain   : 0x%02X\n", SYSTEM_EPOT_GAIN_I2C_ADDRESS);
    printf("  Offset : 0x%02X\n", SYSTEM_EPOT_OFFSET_I2C_ADDRESS);

    const esp_err_t err = Epot_Init(System_GetI2cBus(),
                                    SYSTEM_EPOT_GAIN_I2C_ADDRESS,
                                    SYSTEM_EPOT_OFFSET_I2C_ADDRESS);
    if (err != ESP_OK)
    {
        FatalBlinkLoop("Epot_Init", err);
    }

    printf("Driver EPOT initialise.\n");
    fflush(stdout);
}

static void InitAndTestAd9833(void)
{
    const ad9833_config_t config =
    {
        .host = SYSTEM_AD9833_SPI_HOST,
        .mosi_gpio = SYSTEM_AD9833_MOSI_GPIO,
        .sclk_gpio = SYSTEM_AD9833_SCLK_GPIO,
        .fsync_gpio = SYSTEM_AD9833_FSYNC_GPIO,
        .spi_clock_hz = SYSTEM_AD9833_SPI_CLOCK_HZ,
        .mclk_hz = SYSTEM_AD9833_MCLK_HZ,
    };

    printf("Initialisation AD9833...\n");
    printf("  MOSI/SDATA : GPIO%d\n", (int)SYSTEM_AD9833_MOSI_GPIO);
    printf("  SCLK       : GPIO%d\n", (int)SYSTEM_AD9833_SCLK_GPIO);
    printf("  FSYNC      : GPIO%d\n", (int)SYSTEM_AD9833_FSYNC_GPIO);

    esp_err_t err = Ad9833_Init(&config);
    if (err != ESP_OK)
    {
        FatalBlinkLoop("Ad9833_Init", err);
    }

    printf("Driver AD9833 initialise.\n");

    /*
     * Le preset fournit la forme d'onde de départ.
     * Sa fréquence sera ensuite remplacée en temps réel par la valeur de l'encodeur.
     */
    err = Signaux_ApplyPreset(SYSTEM_AD9833_TEST_PRESET);
    if (err != ESP_OK)
    {
        gAd9833CommandOk = false;
        printf("Erreur application preset AD9833 : %s\n", esp_err_to_name(err));
        fflush(stdout);
        return;
    }

    gAd9833CommandOk = true;
    printf("Preset %u charge comme forme de signal de depart.\n", SYSTEM_AD9833_TEST_PRESET);
    fflush(stdout);
}

static int32_t ClampLiveControlValue(int32_t value)
{
    if (value < LIVE_CONTROL_MIN_VALUE)
    {
        return LIVE_CONTROL_MIN_VALUE;
    }

    if (value > LIVE_CONTROL_MAX_VALUE)
    {
        return LIVE_CONTROL_MAX_VALUE;
    }

    return value;
}

/*
 * Applique UNE MEME valeur logique aux trois blocs actuellement testés :
 *   - AD9833 : valeur utilisée comme fréquence en Hz
 *   - MCP45HV51 Gain : valeur utilisée comme wiper brut 0..255
 *   - MCP45HV51 Offset : valeur utilisée comme wiper brut 0..255
 *
 * Ce mode est volontairement un test d'intégration brut. Les deux EPOT suivent la même
 * valeur tant que les conversions amplitude/offset physiques ne sont pas calibrées.
 */
static esp_err_t ApplyLiveControlValue(int32_t value)
{
    signaux_signal_t signal;
    esp_err_t err;

    if ((value < LIVE_CONTROL_MIN_VALUE) || (value > LIVE_CONTROL_MAX_VALUE))
    {
        return ESP_ERR_INVALID_ARG;
    }

    err = Signaux_GetCurrent(&signal);
    if (err != ESP_OK)
    {
        return err;
    }

    signal.frequency_hz = (float)value;

    err = Signaux_Apply(&signal);
    if (err != ESP_OK)
    {
        gAd9833CommandOk = false;
        return err;
    }

    gAd9833CommandOk = true;

    err = Signaux_SetGainRaw((uint8_t)value);
    if (err != ESP_OK)
    {
        gEpotCommunicationOk = false;
        return err;
    }

    err = Signaux_SetOffsetRaw((uint8_t)value);
    if (err != ESP_OK)
    {
        gEpotCommunicationOk = false;
        return err;
    }

    gEpotCommunicationOk = true;
    return ESP_OK;
}

/*===============================================================================================
MAIN
===============================================================================================*/
void app_main(void)
{
    esp_err_t err;
    const esp_reset_reason_t reset_reason = esp_reset_reason();
    unsigned long heartbeat_count = 0;
    int32_t last_applied_value = -1;
    int32_t last_displayed_value = -1;
    bool status_led_on = false;
    int64_t last_led_toggle_us;
    int64_t last_heartbeat_us;
    bool button_last_raw_state;
    bool button_stable_state;
    int64_t button_last_change_us;

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
    InitEncoder();
    InitEpot();
    InitAndTestAd9833();

    /* Valeur commune de départ affichée et appliquée à toute la chaîne de test. */
    err = Encodeur_SetCounter(LIVE_CONTROL_INITIAL_VALUE);
    if (err != ESP_OK)
    {
        FatalBlinkLoop("Encodeur_SetCounter", err);
    }

    err = ApplyLiveControlValue(LIVE_CONTROL_INITIAL_VALUE);
    if (err != ESP_OK)
    {
        FatalBlinkLoop("ApplyLiveControlValue initial", err);
    }
    last_applied_value = LIVE_CONTROL_INITIAL_VALUE;

    if (Oled_IsInitialized())
    {
        err = Oled_ShowCounter(LIVE_CONTROL_INITIAL_VALUE);
        if (err == ESP_OK)
        {
            gOledCommunicationOk = true;
            last_displayed_value = LIVE_CONTROL_INITIAL_VALUE;
        }
        else
        {
            gOledCommunicationOk = false;
            printf("Erreur affichage valeur initiale : %s\n", esp_err_to_name(err));
        }
    }

    printf("\nControle live actif.\n");
    printf("Encodeur 0..255 -> OLED -> AD9833 (Hz) -> MCP Gain + MCP Offset (raw).\n");
    printf("Exemple : ecran 120 => AD9833 120 Hz, Gain = 120/255, Offset = 120/255.\n");
    printf("Bouton SW : remise de la valeur a 0.\n");
    fflush(stdout);

    last_led_toggle_us = esp_timer_get_time();
    last_heartbeat_us = last_led_toggle_us;

    button_last_raw_state = Encodeur_IsButtonPressed();
    button_stable_state = button_last_raw_state;
    button_last_change_us = last_led_toggle_us;

    while (1)
    {
        const int64_t now_us = esp_timer_get_time();
        const bool button_raw_state = Encodeur_IsButtonPressed();
        int32_t counter;
        int32_t clamped_counter;

        /* Debounce logiciel du bouton de l'encodeur. */
        if (button_raw_state != button_last_raw_state)
        {
            button_last_raw_state = button_raw_state;
            button_last_change_us = now_us;
        }

        if ((button_raw_state != button_stable_state) &&
            ((now_us - button_last_change_us) >= ENCODER_BUTTON_DEBOUNCE_US))
        {
            button_stable_state = button_raw_state;

            if (button_stable_state)
            {
                err = Encodeur_SetCounter(LIVE_CONTROL_MIN_VALUE);
                if (err == ESP_OK)
                {
                    printf("Bouton SW appuye : valeur remise a %ld.\n",
                           (long)LIVE_CONTROL_MIN_VALUE);
                }
                else
                {
                    printf("Erreur remise a zero : %s\n", esp_err_to_name(err));
                }
                fflush(stdout);
            }
        }

        /* Lecture de la nouvelle consigne donnée par l'encodeur. */
        counter = Encodeur_GetCounter();
        clamped_counter = ClampLiveControlValue(counter);

        if (clamped_counter != counter)
        {
            err = Encodeur_SetCounter(clamped_counter);
            if (err != ESP_OK)
            {
                printf("Erreur limitation compteur : %s\n", esp_err_to_name(err));
            }
            counter = clamped_counter;
        }

        /*
         * Une modification de la valeur est appliquée d'abord au hardware :
         * AD9833, MCP Gain puis MCP Offset. L'écran n'est rafraîchi qu'après succès afin que
         * la valeur affichée corresponde toujours à la dernière consigne réellement envoyée.
         */
        if (counter != last_applied_value)
        {
            err = ApplyLiveControlValue(counter);
            if (err == ESP_OK)
            {
                last_applied_value = counter;
                printf("Valeur appliquee : %ld -> AD9833 %.0f Hz -> Gain %ld/255 -> Offset %ld/255\n",
                       (long)counter,
                       (double)Ad9833_GetFrequencyHz(),
                       (long)counter,
                       (long)counter);
            }
            else
            {
                printf("Erreur application valeur %ld : %s\n",
                       (long)counter,
                       esp_err_to_name(err));
            }
            fflush(stdout);
        }

        if ((last_applied_value >= LIVE_CONTROL_MIN_VALUE) &&
            (last_applied_value != last_displayed_value) &&
            Oled_IsInitialized())
        {
            err = Oled_ShowCounter(last_applied_value);
            if (err == ESP_OK)
            {
                gOledCommunicationOk = true;
                last_displayed_value = last_applied_value;
            }
            else
            {
                gOledCommunicationOk = false;
                printf("Erreur mise a jour OLED : %s\n", esp_err_to_name(err));
                fflush(stdout);
            }
        }

        /* LED de vie : changement d'état toutes les 500 ms. */
        if ((now_us - last_led_toggle_us) >= 500000LL)
        {
            status_led_on = !status_led_on;
            StatusLedSet(status_led_on ? 1 : 0);
            last_led_toggle_us = now_us;
        }

        /* Diagnostic série léger chaque seconde. */
        if ((now_us - last_heartbeat_us) >= 1000000LL)
        {
            heartbeat_count++;

            printf("Heartbeat #%lu - valeur=%ld - AD9833=%.0fHz - EPOTs=%s - OLED=%s\n",
                   heartbeat_count,
                   (long)last_applied_value,
                   Ad9833_GetFrequencyHz(),
                   gEpotCommunicationOk ? "OK" : "ECHEC",
                   gOledCommunicationOk ? "OK" : "ECHEC");
            fflush(stdout);

            if ((heartbeat_count % 5UL) == 0UL)
            {
                PrintSystemStatus(reset_reason, heartbeat_count);
            }

            last_heartbeat_us = now_us;
        }

        vTaskDelay(pdMS_TO_TICKS(SYSTEM_ENCODER_POLL_PERIOD_MS));
    }
}

/*===============================================================================================
FIN DU MAIN
===============================================================================================*/
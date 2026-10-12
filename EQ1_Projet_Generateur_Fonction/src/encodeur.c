/*===============================================================================================
CFPT - Projet : Générateur de fonction
Fichier      : encodeur.c
Description  : Décodage du PEC12R par polling périodique des GPIO
===============================================================================================*/

#include "encodeur.h"

#include "freertos/FreeRTOS.h"
#include "esp_timer.h"

/*===============================================================================================
CONSTANTES LOCALES
===============================================================================================*/
#define ENCODEUR_POLL_PERIOD_US                  1000ULL
#define ENCODEUR_EDGE_DEBOUNCE_US                3000LL

/*===============================================================================================
VARIABLES LOCALES
===============================================================================================*/
static gpio_num_t gPinA = GPIO_NUM_NC;
static gpio_num_t gPinB = GPIO_NUM_NC;
static gpio_num_t gPinButton = GPIO_NUM_NC;

static volatile int32_t gCounter = 0;
static volatile int8_t gPulseAccumulator = 0;

static uint8_t gEdgesPerStep = 1U;
static bool gReverseDirection = false;
static bool gEncodeurInitialized = false;

static uint8_t gLastAState = 1U;
static int64_t gLastAcceptedEdgeUs = 0LL;
static esp_timer_handle_t gPollTimer = NULL;

static portMUX_TYPE gEncodeurMux = portMUX_INITIALIZER_UNLOCKED;

/*===============================================================================================
PROTOTYPES DE FONCTIONS LOCALES
===============================================================================================*/
static void Encodeur_PollTimerCallback(void *arg);
static void Encodeur_ResetState(void);

/*===============================================================================================
FONCTIONS LOCALES
===============================================================================================*/
/*
 * Le PEC12R est lu directement toutes les 1 ms.
 * On compte uniquement le front montant de A et on lit B pour connaître le sens.
 * Cette approche évite toute dépendance au service d'interruption GPIO et permet
 * de vérifier directement les niveaux réellement présents sur GPIO1 / GPIO2.
 */
static void Encodeur_PollTimerCallback(void *arg)
{
    (void)arg;

    if (!gEncodeurInitialized)
    {
        return;
    }

    const uint8_t a = (uint8_t)gpio_get_level(gPinA);
    const uint8_t b = (uint8_t)gpio_get_level(gPinB);
    const int64_t now_us = esp_timer_get_time();

    /* Front montant A détecté. */
    if ((gLastAState == 0U) && (a != 0U))
    {
        if ((now_us - gLastAcceptedEdgeUs) >= ENCODEUR_EDGE_DEBOUNCE_US)
        {
            int8_t direction = (b != 0U) ? 1 : -1;

            if (gReverseDirection)
            {
                direction = (int8_t)-direction;
            }

            portENTER_CRITICAL(&gEncodeurMux);

            gPulseAccumulator = (int8_t)(gPulseAccumulator + direction);

            if (gPulseAccumulator >= (int8_t)gEdgesPerStep)
            {
                gCounter++;
                gPulseAccumulator = 0;
            }
            else if (gPulseAccumulator <= -(int8_t)gEdgesPerStep)
            {
                gCounter--;
                gPulseAccumulator = 0;
            }

            portEXIT_CRITICAL(&gEncodeurMux);

            gLastAcceptedEdgeUs = now_us;
        }
    }

    gLastAState = a;
}

static void Encodeur_ResetState(void)
{
    gPinA = GPIO_NUM_NC;
    gPinB = GPIO_NUM_NC;
    gPinButton = GPIO_NUM_NC;
    gEdgesPerStep = 1U;
    gReverseDirection = false;
    gEncodeurInitialized = false;
    gLastAState = 1U;
    gLastAcceptedEdgeUs = 0LL;

    portENTER_CRITICAL(&gEncodeurMux);
    gCounter = 0;
    gPulseAccumulator = 0;
    portEXIT_CRITICAL(&gEncodeurMux);
}

/*===============================================================================================
FONCTIONS PUBLIQUES
===============================================================================================*/
esp_err_t Encodeur_Init(gpio_num_t pin_a,
                        gpio_num_t pin_b,
                        gpio_num_t pin_button,
                        uint8_t edges_per_step,
                        bool reverse_direction)
{
    esp_err_t err;

    if (gEncodeurInitialized)
    {
        return ESP_OK;
    }

    if (!GPIO_IS_VALID_GPIO(pin_a) ||
        !GPIO_IS_VALID_GPIO(pin_b) ||
        !GPIO_IS_VALID_GPIO(pin_button) ||
        (pin_a == pin_b) ||
        (pin_a == pin_button) ||
        (pin_b == pin_button) ||
        (edges_per_step == 0U))
    {
        return ESP_ERR_INVALID_ARG;
    }

    gPinA = pin_a;
    gPinB = pin_b;
    gPinButton = pin_button;
    gEdgesPerStep = edges_per_step;
    gReverseDirection = reverse_direction;

    const gpio_config_t encoder_config =
    {
        .pin_bit_mask = (1ULL << (uint32_t)gPinA) |
                        (1ULL << (uint32_t)gPinB),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    err = gpio_config(&encoder_config);
    if (err != ESP_OK)
    {
        Encodeur_ResetState();
        return err;
    }

    const gpio_config_t button_config =
    {
        .pin_bit_mask = (1ULL << (uint32_t)gPinButton),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    err = gpio_config(&button_config);
    if (err != ESP_OK)
    {
        Encodeur_ResetState();
        return err;
    }

    portENTER_CRITICAL(&gEncodeurMux);
    gCounter = 0;
    gPulseAccumulator = 0;
    portEXIT_CRITICAL(&gEncodeurMux);

    gLastAState = (uint8_t)gpio_get_level(gPinA);
    gLastAcceptedEdgeUs = esp_timer_get_time() - ENCODEUR_EDGE_DEBOUNCE_US;

    const esp_timer_create_args_t timer_args =
    {
        .callback = Encodeur_PollTimerCallback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "encoder_poll",
        .skip_unhandled_events = true,
    };

    err = esp_timer_create(&timer_args, &gPollTimer);
    if (err != ESP_OK)
    {
        Encodeur_ResetState();
        return err;
    }

    gEncodeurInitialized = true;

    err = esp_timer_start_periodic(gPollTimer, ENCODEUR_POLL_PERIOD_US);
    if (err != ESP_OK)
    {
        gEncodeurInitialized = false;
        esp_timer_delete(gPollTimer);
        gPollTimer = NULL;
        Encodeur_ResetState();
        return err;
    }

    return ESP_OK;
}

esp_err_t Encodeur_Deinit(void)
{
    esp_err_t first_error = ESP_OK;

    if (!gEncodeurInitialized)
    {
        return ESP_OK;
    }

    gEncodeurInitialized = false;

    if (gPollTimer != NULL)
    {
        esp_err_t err = esp_timer_stop(gPollTimer);
        if ((err != ESP_OK) && (err != ESP_ERR_INVALID_STATE))
        {
            first_error = err;
        }

        err = esp_timer_delete(gPollTimer);
        if ((err != ESP_OK) && (first_error == ESP_OK))
        {
            first_error = err;
        }

        gPollTimer = NULL;
    }

    gpio_reset_pin(gPinA);
    gpio_reset_pin(gPinB);
    gpio_reset_pin(gPinButton);

    Encodeur_ResetState();
    return first_error;
}

int32_t Encodeur_GetCounter(void)
{
    int32_t counter;

    portENTER_CRITICAL(&gEncodeurMux);
    counter = gCounter;
    portEXIT_CRITICAL(&gEncodeurMux);

    return counter;
}

esp_err_t Encodeur_SetCounter(int32_t value)
{
    if (!gEncodeurInitialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    portENTER_CRITICAL(&gEncodeurMux);
    gCounter = value;
    gPulseAccumulator = 0;
    portEXIT_CRITICAL(&gEncodeurMux);

    return ESP_OK;
}

bool Encodeur_IsInitialized(void)
{
    return gEncodeurInitialized;
}

bool Encodeur_IsButtonPressed(void)
{
    if (!gEncodeurInitialized)
    {
        return false;
    }

    return gpio_get_level(gPinButton) == 0;
}

/*===============================================================================================
FIN DU FICHIER
===============================================================================================*/

/*===============================================================================================
CFPT - Projet : Générateur de fonction
Fichier      : encodeur.c
Description  : Décodage quadrature du PEC12R sous interruption GPIO
===============================================================================================*/

#include "encodeur.h"

#include "freertos/FreeRTOS.h"

/*===============================================================================================
VARIABLES LOCALES
===============================================================================================*/
static gpio_num_t gPinA = GPIO_NUM_NC;
static gpio_num_t gPinB = GPIO_NUM_NC;
static gpio_num_t gPinButton = GPIO_NUM_NC;

static volatile int32_t gCounter = 0;
static volatile int8_t gTransitionAccumulator = 0;
static volatile uint8_t gLastState = 0;

static uint8_t gEdgesPerStep = 4U;
static bool gReverseDirection = false;
static bool gEncodeurInitialized = false;
static bool gOwnsIsrService = false;

static portMUX_TYPE gEncodeurMux = portMUX_INITIALIZER_UNLOCKED;

/*
 * Table de décodage quadrature.
 * Index = (ancien_etat << 2) | nouvel_etat
 * Etats A/B codés sur 2 bits.
 * Les transitions invalides ou dues à un rebond retournent 0.
 */
static const int8_t gTransitionTable[16] =
{
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};

/*===============================================================================================
PROTOTYPES DE FONCTIONS LOCALES
===============================================================================================*/
static void Encodeur_IsrHandler(void *arg);
static uint8_t Encodeur_ReadState(void);
static void Encodeur_ResetState(void);

/*===============================================================================================
FONCTIONS LOCALES
===============================================================================================*/
static uint8_t Encodeur_ReadState(void)
{
    const uint8_t a = (uint8_t)gpio_get_level(gPinA);
    const uint8_t b = (uint8_t)gpio_get_level(gPinB);

    return (uint8_t)((a << 1U) | b);
}

static void Encodeur_IsrHandler(void *arg)
{
    (void)arg;

    const uint8_t new_state = Encodeur_ReadState();
    int8_t delta;

    portENTER_CRITICAL_ISR(&gEncodeurMux);

    delta = gTransitionTable[(gLastState << 2U) | new_state];
    gLastState = new_state;

    if (gReverseDirection)
    {
        delta = (int8_t)-delta;
    }

    if (delta != 0)
    {
        gTransitionAccumulator = (int8_t)(gTransitionAccumulator + delta);

        if (gTransitionAccumulator >= (int8_t)gEdgesPerStep)
        {
            gCounter++;
            gTransitionAccumulator = 0;
        }
        else if (gTransitionAccumulator <= -(int8_t)gEdgesPerStep)
        {
            gCounter--;
            gTransitionAccumulator = 0;
        }
    }

    portEXIT_CRITICAL_ISR(&gEncodeurMux);
}

static void Encodeur_ResetState(void)
{
    gPinA = GPIO_NUM_NC;
    gPinB = GPIO_NUM_NC;
    gPinButton = GPIO_NUM_NC;
    gEdgesPerStep = 4U;
    gReverseDirection = false;
    gEncodeurInitialized = false;
    gOwnsIsrService = false;

    portENTER_CRITICAL(&gEncodeurMux);
    gCounter = 0;
    gTransitionAccumulator = 0;
    gLastState = 0;
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
        .intr_type = GPIO_INTR_ANYEDGE,
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
    gTransitionAccumulator = 0;
    gLastState = Encodeur_ReadState();
    portEXIT_CRITICAL(&gEncodeurMux);

    err = gpio_install_isr_service(0);
    if (err == ESP_OK)
    {
        gOwnsIsrService = true;
    }
    else if (err == ESP_ERR_INVALID_STATE)
    {
        /* Le service ISR GPIO existe déjà : on le réutilise. */
        gOwnsIsrService = false;
    }
    else
    {
        Encodeur_ResetState();
        return err;
    }

    err = gpio_isr_handler_add(gPinA, Encodeur_IsrHandler, NULL);
    if (err != ESP_OK)
    {
        if (gOwnsIsrService)
        {
            gpio_uninstall_isr_service();
        }
        Encodeur_ResetState();
        return err;
    }

    err = gpio_isr_handler_add(gPinB, Encodeur_IsrHandler, NULL);
    if (err != ESP_OK)
    {
        gpio_isr_handler_remove(gPinA);
        if (gOwnsIsrService)
        {
            gpio_uninstall_isr_service();
        }
        Encodeur_ResetState();
        return err;
    }

    gEncodeurInitialized = true;
    return ESP_OK;
}

esp_err_t Encodeur_Deinit(void)
{
    esp_err_t first_error = ESP_OK;
    esp_err_t err;

    if (!gEncodeurInitialized)
    {
        return ESP_OK;
    }

    err = gpio_isr_handler_remove(gPinA);
    if ((err != ESP_OK) && (first_error == ESP_OK))
    {
        first_error = err;
    }

    err = gpio_isr_handler_remove(gPinB);
    if ((err != ESP_OK) && (first_error == ESP_OK))
    {
        first_error = err;
    }

    if (gOwnsIsrService)
    {
        gpio_uninstall_isr_service();
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
    gTransitionAccumulator = 0;
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

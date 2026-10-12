/*===============================================================================================
CFPT - Projet : Générateur de fonction
Fichier      : encodeur.c
Description  : Décodage du PEC12R sous interruption GPIO
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
static volatile int8_t gPulseAccumulator = 0;

static uint8_t gEdgesPerStep = 1U;
static bool gReverseDirection = false;
static bool gEncodeurInitialized = false;
static bool gOwnsIsrService = false;

static portMUX_TYPE gEncodeurMux = portMUX_INITIALIZER_UNLOCKED;

/*===============================================================================================
PROTOTYPES DE FONCTIONS LOCALES
===============================================================================================*/
static void Encodeur_IsrHandler(void *arg);
static void Encodeur_ResetState(void);

/*===============================================================================================
FONCTIONS LOCALES
===============================================================================================*/
/*
 * Le PEC12R-4220F-S0024 fournit 24 impulsions par tour.
 * Pour le prototype, on compte uniquement le front montant du canal A.
 * L'état du canal B au même instant donne le sens de rotation.
 *
 * Cette méthode est volontairement plus simple et plus robuste sur un encodeur
 * mécanique à contacts que l'ancien décodage qui exigeait quatre transitions
 * quadrature valides avant de modifier le compteur.
 */
static void IRAM_ATTR Encodeur_IsrHandler(void *arg)
{
    (void)arg;

    const int level_b = gpio_get_level(gPinB);
    int8_t direction = (level_b != 0) ? 1 : -1;

    if (gReverseDirection)
    {
        direction = (int8_t)-direction;
    }

    portENTER_CRITICAL_ISR(&gEncodeurMux);

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

    portEXIT_CRITICAL_ISR(&gEncodeurMux);
}

static void Encodeur_ResetState(void)
{
    gPinA = GPIO_NUM_NC;
    gPinB = GPIO_NUM_NC;
    gPinButton = GPIO_NUM_NC;
    gEdgesPerStep = 1U;
    gReverseDirection = false;
    gEncodeurInitialized = false;
    gOwnsIsrService = false;

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

    /*
     * A déclenche l'interruption sur front montant.
     * B est lu dans l'ISR pour déterminer le sens.
     */
    const gpio_config_t encoder_a_config =
    {
        .pin_bit_mask = (1ULL << (uint32_t)gPinA),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };

    err = gpio_config(&encoder_a_config);
    if (err != ESP_OK)
    {
        Encodeur_ResetState();
        return err;
    }

    const gpio_config_t encoder_b_config =
    {
        .pin_bit_mask = (1ULL << (uint32_t)gPinB),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    err = gpio_config(&encoder_b_config);
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

#include "encodeur.h"

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_timer.h"

#define REBOND_BOUTON_US 30000LL
#define APPUI_LONG_US    800000LL

/*
 * Decodeur quadrature robuste pour le PEC12R.
 *
 * Principe important : on ne genere PAS un evenement pendant le mouvement.
 * Un cran n'est valide que lorsque l'encodeur a quitte sa position de detente,
 * effectue une sequence quadrature complete, puis est revenu sur cette meme
 * position de detente.
 *
 * Cela elimine les faux aller/retour provoques par le rebond mecanique autour
 * d'un cran.
 */
static const int8_t transitions[16] =
{
     0, +1, -1,  0,
    -1,  0,  0, +1,
    +1,  0,  0, -1,
     0, -1, +1,  0
};

static QueueHandle_t queue_ab;
static uint8_t ab_precedent;
static uint8_t etat_detente;
static int8_t quadrature_somme;
static int rotation_en_attente;

static bool bouton_brut_precedent;
static bool bouton_stable;
static bool appui_long_declenche;
static int64_t heure_rebond;
static int64_t heure_appui;

static uint8_t lire_ab(void)
{
    return (uint8_t)((gpio_get_level(PIN_ENC_A) << 1) |
                     gpio_get_level(PIN_ENC_B));
}

static void IRAM_ATTR interruption_ab(void *unused)
{
    (void)unused;

    const uint8_t ab = (uint8_t)((gpio_get_level(PIN_ENC_A) << 1) |
                                 gpio_get_level(PIN_ENC_B));

    BaseType_t reveil = pdFALSE;
    (void)xQueueSendFromISR(queue_ab, &ab, &reveil);

    if (reveil == pdTRUE)
    {
        portYIELD_FROM_ISR();
    }
}

void encodeur_initialiser(void)
{
    queue_ab = xQueueCreate(64, sizeof(uint8_t));
    configASSERT(queue_ab != NULL);

    const gpio_config_t rotation =
    {
        .pin_bit_mask = (1ULL << PIN_ENC_A) | (1ULL << PIN_ENC_B),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };

    const gpio_config_t clic =
    {
        .pin_bit_mask = 1ULL << PIN_ENC_SW,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(gpio_config(&rotation));
    ESP_ERROR_CHECK(gpio_config(&clic));

    /*
     * Au demarrage l'encodeur est normalement pose sur un cran.
     * On prend donc son etat A/B courant comme position de detente de reference.
     * Cette methode fonctionne que la detente electrique soit 00 ou 11.
     */
    etat_detente = lire_ab();
    ab_precedent = etat_detente;
    quadrature_somme = 0;
    rotation_en_attente = 0;

    bouton_brut_precedent = (gpio_get_level(PIN_ENC_SW) == 0);
    bouton_stable = bouton_brut_precedent;
    appui_long_declenche = false;
    heure_rebond = esp_timer_get_time();
    heure_appui = heure_rebond;

    const esp_err_t isr_err = gpio_install_isr_service(0);
    if ((isr_err != ESP_OK) && (isr_err != ESP_ERR_INVALID_STATE))
    {
        ESP_ERROR_CHECK(isr_err);
    }

    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_ENC_A, interruption_ab, NULL));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_ENC_B, interruption_ab, NULL));
}

static void traiter_transition(uint8_t ab)
{
    if (ab == ab_precedent)
    {
        return;
    }

    const int8_t pas = transitions[(ab_precedent << 2) | ab];

    if (pas == 0)
    {
        /*
         * Deux bits ont change d'un coup : sequence invalide ou fronts perdus.
         * On annule le mouvement en cours plutot que de risquer un faux cran.
         */
        quadrature_somme = 0;
    }
    else
    {
        quadrature_somme = (int8_t)(quadrature_somme + pas);
    }

    ab_precedent = ab;

    /*
     * Un evenement UI n'est emis qu'au RETOUR sur la position de detente.
     * Une sequence complete vaut +/-4 transitions valides.
     * Les rebonds partiels autour du cran reviennent avec une somme proche de 0
     * et ne provoquent donc aucun deplacement du menu.
     */
    if (ab == etat_detente)
    {
        if (quadrature_somme >= 4)
        {
#if INVERSION_SENS_ENCODEUR
            rotation_en_attente--;
#else
            rotation_en_attente++;
#endif
        }
        else if (quadrature_somme <= -4)
        {
#if INVERSION_SENS_ENCODEUR
            rotation_en_attente++;
#else
            rotation_en_attente--;
#endif
        }

        quadrature_somme = 0;
    }
}

static void traiter_file_rotation(TickType_t attente)
{
    uint8_t ab;

    if (xQueueReceive(queue_ab, &ab, attente) != pdTRUE)
    {
        return;
    }

    traiter_transition(ab);

    while (xQueueReceive(queue_ab, &ab, 0) == pdTRUE)
    {
        traiter_transition(ab);
    }
}

evenement_ui_t encodeur_lire_evenement(void)
{
    /* Attente courte pour eviter une boucle CPU a 100 %. */
    traiter_file_rotation(pdMS_TO_TICKS(10));

    const bool brut = (gpio_get_level(PIN_ENC_SW) == 0);
    const int64_t maintenant = esp_timer_get_time();

    if (brut != bouton_brut_precedent)
    {
        bouton_brut_precedent = brut;
        heure_rebond = maintenant;
    }

    if ((brut != bouton_stable) &&
        ((maintenant - heure_rebond) >= REBOND_BOUTON_US))
    {
        bouton_stable = brut;

        if (bouton_stable)
        {
            heure_appui = maintenant;
            appui_long_declenche = false;
        }
        else if (!appui_long_declenche)
        {
            return UI_CLIC;
        }
    }

    if (bouton_stable && !appui_long_declenche &&
        ((maintenant - heure_appui) >= APPUI_LONG_US))
    {
        appui_long_declenche = true;
        return UI_APPUI_LONG;
    }

    if (rotation_en_attente != 0)
    {
        const int sens = (rotation_en_attente > 0) ? +1 : -1;
        rotation_en_attente -= sens;
        return (sens > 0) ? UI_ROTATION_PLUS : UI_ROTATION_MOINS;
    }

    return UI_AUCUN;
}

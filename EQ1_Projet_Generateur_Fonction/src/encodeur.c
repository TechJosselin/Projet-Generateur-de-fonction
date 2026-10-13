#include "encodeur.h"

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_timer.h"

/*
 * PEC12R-4220F-S0024 : 24 detentes / 24 impulsions par tour.
 * Le constructeur annonce jusqu'a 5 ms de rebond de contact.
 * On genere donc UN evenement de rotation sur un seul front montant de A,
 * puis on ignore les fronts parasites pendant 6 ms.
 */
#define REBOND_ROTATION_US 6000LL
#define REBOND_BOUTON_US   30000LL
#define APPUI_LONG_US      800000LL

typedef struct
{
    int8_t sens;
    int64_t instant_us;
} rotation_isr_t;

static QueueHandle_t queue_rotation;
static int rotation_en_attente;
static int64_t derniere_rotation_acceptee_us;

static bool bouton_brut_precedent;
static bool bouton_stable;
static bool appui_long_declenche;
static int64_t heure_rebond;
static int64_t heure_appui;

/*
 * Une seule interruption par impulsion : front montant de A.
 * B donne le sens de rotation au moment du front.
 */
static void interruption_rotation(void *unused)
{
    (void)unused;

    rotation_isr_t evenement = {
        .sens = gpio_get_level(PIN_ENC_B) ? +1 : -1,
        .instant_us = esp_timer_get_time()
    };

    BaseType_t reveil = pdFALSE;
    xQueueSendFromISR(queue_rotation, &evenement, &reveil);

    if (reveil == pdTRUE)
    {
        portYIELD_FROM_ISR();
    }
}

void encodeur_initialiser(void)
{
    queue_rotation = xQueueCreate(32, sizeof(rotation_isr_t));
    configASSERT(queue_rotation != NULL);

    /* Canal A : seul canal interruptif. */
    const gpio_config_t canal_a = {
        .pin_bit_mask = 1ULL << PIN_ENC_A,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE
    };

    /* Canal B : lecture du sens uniquement. */
    const gpio_config_t canal_b = {
        .pin_bit_mask = 1ULL << PIN_ENC_B,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    const gpio_config_t clic = {
        .pin_bit_mask = 1ULL << PIN_ENC_SW,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(gpio_config(&canal_a));
    ESP_ERROR_CHECK(gpio_config(&canal_b));
    ESP_ERROR_CHECK(gpio_config(&clic));

    rotation_en_attente = 0;
    derniere_rotation_acceptee_us = esp_timer_get_time() - REBOND_ROTATION_US;

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

    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_ENC_A, interruption_rotation, NULL));
}

static void traiter_rotation_en_attente(void)
{
    rotation_isr_t evenement;

    while (xQueueReceive(queue_rotation, &evenement, 0) == pdTRUE)
    {
        /*
         * Le PEC12R est un encodeur mecanique. Un meme cran peut produire plusieurs
         * fronts a cause du rebond des contacts. Seul le premier front dans la fenetre
         * de 6 ms est considere comme une vraie impulsion.
         */
        if ((evenement.instant_us - derniere_rotation_acceptee_us) < REBOND_ROTATION_US)
        {
            continue;
        }

        derniere_rotation_acceptee_us = evenement.instant_us;

        int sens = evenement.sens;
#if INVERSION_SENS_ENCODEUR
        sens = -sens;
#endif
        rotation_en_attente += sens;
    }
}

evenement_ui_t encodeur_lire_evenement(void)
{
    /*
     * Attente courte pour eviter une boucle CPU a 100 %, puis vidage de toutes
     * les interruptions de rotation recues depuis le dernier passage.
     */
    rotation_isr_t premier;
    if (xQueueReceive(queue_rotation, &premier, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        if ((premier.instant_us - derniere_rotation_acceptee_us) >= REBOND_ROTATION_US)
        {
            derniere_rotation_acceptee_us = premier.instant_us;
            int sens = premier.sens;
#if INVERSION_SENS_ENCODEUR
            sens = -sens;
#endif
            rotation_en_attente += sens;
        }

        traiter_rotation_en_attente();
    }

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

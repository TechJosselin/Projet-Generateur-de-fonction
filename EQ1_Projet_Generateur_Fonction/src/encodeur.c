/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : ENCODEUR.C
 * DESCRIPTIF : Interruption et acquisition de l'encodeur rotatif.
 *==================================================================================================*/
/* Decodeur quadrature du prototype, extrait et teste dans entree.c. */
#include "encodeur.h"
#include "hardware_config.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "temps.h"

/*============================== VARIABLES ET CONFIGURATION =======================================*/
#define ENCODEUR_CAPACITE_FILE          128U
#define ENCODEUR_TRANSITIONS_PAR_APPEL  128U
#define ENCODEUR_ROTATIONS_MAX_ATTENTE  128
static QueueHandle_t file_evenements;
static quadrature_t etat_quadrature;
static bouton_t etat_bouton;
static int rotations_en_attente;
static uint32_t transitions_perdues, pertes_observees;
static bool initialise;
static portMUX_TYPE verrou = portMUX_INITIALIZER_UNLOCKED;
/*==================================== FONCTIONS ================================================*/
static uint8_t lire_etat_ab(void)
{
    return(uint8_t)((gpio_get_level(SYSTEM_ENCODER_A_GPIO) << 1) | gpio_get_level(SYSTEM_ENCODER_B_GPIO));
}

static void interruption(void *unused)
{
    (void) unused;
    uint8_t etat_ab = lire_etat_ab();
    BaseType_t reveil = pdFALSE;
    if (xQueueSendFromISR(file_evenements, &etat_ab, &reveil) != pdTRUE)
    {
        portENTER_CRITICAL_ISR(&verrou);
        transitions_perdues++;
        portEXIT_CRITICAL_ISR(&verrou);
    }
    if (reveil)
    {
        portYIELD_FROM_ISR();
    }
}

/*================================ INTERFACE PUBLIQUE ============================================*/
uint32_t encodeur_pertes(void)
{
    portENTER_CRITICAL(&verrou);
    uint32_t v = transitions_perdues;
    portEXIT_CRITICAL(&verrou);
    return v;
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : encodeur_initialiser()
 * Descriptif : Configure les broches et les interruptions.
 *--------------------------------------------------------------------------------------------------*/
bool encodeur_initialiser(void)
{
    if (initialise)
    {
        return true;
    }
    file_evenements = xQueueCreate(ENCODEUR_CAPACITE_FILE, sizeof(uint8_t));
    if (!file_evenements)
    {
        return false;
    }
    gpio_config_t g = {
        .pin_bit_mask =(1ULL << SYSTEM_ENCODER_A_GPIO) |(1ULL << SYSTEM_ENCODER_B_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    esp_err_t e = gpio_config(&g);
    if (e != ESP_OK)
    {
        goto fail;
    }
    g.pin_bit_mask = 1ULL << SYSTEM_ENCODER_BUTTON_GPIO;
    g.intr_type = GPIO_INTR_DISABLE;
    e = gpio_config(&g);
    if (e != ESP_OK)
    {
        goto fail;
    }
    quadrature_initialiser(&etat_quadrature, lire_etat_ab());
    bouton_initialiser(&etat_bouton, gpio_get_level(SYSTEM_ENCODER_BUTTON_GPIO) == 0, (uint32_t)(esp_timer_get_time() / TEMPS_US_PAR_MS));
    e = gpio_install_isr_service(0);
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE)
    {
        goto fail;
    }
    e = gpio_isr_handler_add(SYSTEM_ENCODER_A_GPIO, interruption, NULL);
    if (e != ESP_OK)
    {
        goto fail;
    }
    e = gpio_isr_handler_add(SYSTEM_ENCODER_B_GPIO, interruption, NULL);
    if (e != ESP_OK)
    {
        (void) gpio_isr_handler_remove(SYSTEM_ENCODER_A_GPIO);
        goto fail;
    }
    initialise = true;
    return true;
    fail : vQueueDelete(file_evenements);
    file_evenements = NULL;
    return false;
}

evenement_ui_t encodeur_lire_evenement(void)
{
    if (!initialise)
    {
        return UI_AUCUN;
    }
    uint32_t nombre_pertes = encodeur_pertes();
    if (nombre_pertes != pertes_observees)
    {
        pertes_observees = nombre_pertes;
        xQueueReset(file_evenements);
        quadrature_initialiser(&etat_quadrature, lire_etat_ab());
        rotations_en_attente = 0;
    }
    uint8_t etat_ab;
    unsigned transitions_traitees = 0;
    while (transitions_traitees++ < ENCODEUR_TRANSITIONS_PAR_APPEL && xQueueReceive(file_evenements, &etat_ab, 0) == pdTRUE)
    {
        int delta_rotation = quadrature_traiter(&etat_quadrature, etat_ab);
        if (SYSTEM_ENCODER_REVERSE_DIRECTION)
        {
            delta_rotation = -delta_rotation;
        }
        rotations_en_attente += delta_rotation;
        if (rotations_en_attente > ENCODEUR_ROTATIONS_MAX_ATTENTE)
        {
            rotations_en_attente = ENCODEUR_ROTATIONS_MAX_ATTENTE;
        }
        if (rotations_en_attente < -ENCODEUR_ROTATIONS_MAX_ATTENTE)
        {
            rotations_en_attente = -ENCODEUR_ROTATIONS_MAX_ATTENTE;
        }
    }
    evenement_ui_t e = bouton_traiter(&etat_bouton, gpio_get_level(SYSTEM_ENCODER_BUTTON_GPIO) == 0, (uint32_t)(esp_timer_get_time() / TEMPS_US_PAR_MS));
    if (e != UI_AUCUN)
    {
        return e;
    }
    if (rotations_en_attente > 0)
    {
        rotations_en_attente--;
        return UI_ROTATION_PLUS;
    }
    if (rotations_en_attente < 0)
    {
        rotations_en_attente++;
        return UI_ROTATION_MOINS;
    }
    return UI_AUCUN;
}

/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : SORTIES_DIGITALES.C
 * DESCRIPTIF : Pilotage de deux sorties numeriques avec LEDC.
 *==================================================================================================*/
#include "sorties_digitales.h"
#include "hardware_config.h"
#include "board_options.h"
#include "driver/ledc.h"

/*============================== VARIABLES ET CONFIGURATION =======================================*/
#define DIGITAL_LEDC_RESOLUTION_INITIALE_BITS 14U
#define DIGITAL_LEDC_RESOLUTION_MIN_BITS      1U
#define DIGITAL_LEDC_HORLOGE_REFERENCE_HZ     40000000U
#define DIGITAL_LEDC_ERREUR_MAX_POURCENT      1U
#define DIGITAL_POURCENT_BASE               100U
static const int pins[DIGITAL_NOMBRE_CANAUX] = { SYSTEM_DIGITAL_1_GPIO, SYSTEM_DIGITAL_2_GPIO };
static bool disponibles[DIGITAL_NOMBRE_CANAUX], configures[DIGITAL_NOMBRE_CANAUX];
/*==================================== FONCTIONS ================================================*/
static bool pin_autorise(int p)
{
    if (!GPIO_IS_VALID_OUTPUT_GPIO(p))
    {
        return false;
    }
    /* USB, flash, straps, LED et autres fonctions existantes restent reserves. */
    const int reserves[] = {
        0,
        1,
        2,
        4,
        5,
        8,
        9,
        12,
        13,
        15,
        18,
        19,
        21,
        22,
        23,
        24,
        25,
        26,
        27,
        28,
        29,
        30
    };
    for (unsigned i = 0; i < sizeof(reserves) / sizeof(reserves[0]); i++)
    {
        if (p == reserves[i])
        {
            return false;
        }
    }
    return true;
}

/*================================ INTERFACE PUBLIQUE ============================================*/
void digitales_initialiser(void)
{
    for (unsigned canal = 0; canal < DIGITAL_NOMBRE_CANAUX; canal++)
    {
        disponibles[canal] = false;
        configures[canal] = false;
        if (pins[0] == pins[1] || !pin_autorise(pins[canal]))
        {
            continue;
        }
        gpio_config_t configuration_gpio = {
            .pin_bit_mask = 1ULL << pins[canal],
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        if (gpio_set_level(pins[canal], 0) == ESP_OK && gpio_config(&configuration_gpio) == ESP_OK)
        {
            disponibles[canal] = true;
        }
    }
}

bool digitales_disponible(unsigned canal)
{
    return canal < DIGITAL_NOMBRE_CANAUX && disponibles[canal];
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : digitales_commander()
 * Descriptif : Configure ou arrete un canal PWM LEDC.
 *--------------------------------------------------------------------------------------------------*/
gf_result_t digitales_commander(unsigned canal, uint32_t frequence_hz, uint16_t rapport_pm, bool activer)
{
    if (canal >= DIGITAL_NOMBRE_CANAUX || rapport_pm > DIGITAL_RAPPORT_MAX_PM)
    {
        return GF_ARGUMENT;
    }
    if (!disponibles[canal])
    {
        return GF_NON_SUPPORTE;
    }
    if (!activer)
    {
        esp_err_t erreur_pilote;
        if (configures[canal])
        {
            erreur_pilote = ledc_stop(LEDC_LOW_SPEED_MODE, (ledc_channel_t) canal, 0);
        }
        else
        {
            erreur_pilote = gpio_set_level(pins[canal], 0);
        }
        if (erreur_pilote == ESP_OK)
        {
            return GF_OK;
        }
        return GF_IO;
    }
    if (frequence_hz < DIGITAL_FREQUENCE_MIN_HZ || frequence_hz > DIGITAL_FREQUENCE_MAX_HZ)
    {
        return GF_PLAGE;
    }
    unsigned resolution_bits = DIGITAL_LEDC_RESOLUTION_INITIALE_BITS;
    while (resolution_bits > DIGITAL_LEDC_RESOLUTION_MIN_BITS &&((uint64_t) frequence_hz << resolution_bits) > DIGITAL_LEDC_HORLOGE_REFERENCE_HZ)
    {
        resolution_bits--;
    }
    ledc_timer_config_t configuration_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num =(ledc_timer_t) canal,
        .duty_resolution =(ledc_timer_bit_t) resolution_bits,
        .freq_hz = frequence_hz,
        .clk_cfg = LEDC_USE_XTAL_CLK
    };
    esp_err_t erreur_pilote = ledc_timer_config(&configuration_timer);
    if (erreur_pilote != ESP_OK)
    {
        goto fail;
    }
    uint32_t frequence_reelle_hz = ledc_get_freq(LEDC_LOW_SPEED_MODE, (ledc_timer_t) canal);
    uint32_t ecart_frequence_hz = frequence_hz - frequence_reelle_hz;
    if (frequence_reelle_hz > frequence_hz)
    {
        ecart_frequence_hz = frequence_reelle_hz - frequence_hz;
    }
    if (!frequence_reelle_hz ||(uint64_t) ecart_frequence_hz * DIGITAL_POURCENT_BASE > frequence_hz * DIGITAL_LEDC_ERREUR_MAX_POURCENT)
    {
        erreur_pilote = ESP_ERR_INVALID_ARG;
        goto fail;
    }
    ledc_channel_config_t configuration_canal = {
        .gpio_num = pins[canal],
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel =(ledc_channel_t) canal,
        .timer_sel =(ledc_timer_t) canal,
        .intr_type = LEDC_INTR_DISABLE,
        .rapport_pm = 0,
        .hpoint = 0
    };
    /* Calcule le rapport cyclique sans modifier le traitement LEDC de 100 %. */
    if (rapport_pm != DIGITAL_RAPPORT_MAX_PM)
    {
        configuration_canal.rapport_pm =(uint32_t)((((uint64_t) 1 << resolution_bits) * rapport_pm + (DIGITAL_RAPPORT_MAX_PM / 2U)) / DIGITAL_RAPPORT_MAX_PM);
    }
    /* Arrondir une consigne presque pleine sans depasser le compteur. */
    if (configuration_canal.rapport_pm >=(1U << resolution_bits))
    {
        configuration_canal.rapport_pm =(1U << resolution_bits) - 1;
    }
    erreur_pilote = ledc_channel_config(&configuration_canal);
    if (erreur_pilote != ESP_OK)
    {
        goto fail;
    }
    configures[canal] = true;
    if (rapport_pm == 0 || rapport_pm == DIGITAL_RAPPORT_MAX_PM)
    {
        erreur_pilote = ledc_stop(LEDC_LOW_SPEED_MODE, (ledc_channel_t) canal, rapport_pm == DIGITAL_RAPPORT_MAX_PM);
    }
    if (erreur_pilote == ESP_OK)
    {
        return GF_OK;
    }
    fail : if (configures[canal])
    {
        (void) ledc_stop(LEDC_LOW_SPEED_MODE, (ledc_channel_t) canal, 0);
    }
    if (erreur_pilote == ESP_ERR_INVALID_ARG)
    {
        return GF_PLAGE;
    }
    return GF_IO;
}

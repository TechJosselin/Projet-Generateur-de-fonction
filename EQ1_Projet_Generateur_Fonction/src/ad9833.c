/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : AD9833.C
 * DESCRIPTIF : Pilote SPI du generateur DDS : frequence et RESET.
 *==================================================================================================*/
/* CFPT - Base du pilote : travail de Samuel (RDMR_GenerationSignal).
* Finalisation : PHASE0, entrees finies, transitions explicites et timeout SPI.
* RESET n'est ni une isolation de VOUT, ni une garantie de tension nulle. */
#include "ad9833.h"
#include <math.h>
#include <stddef.h>
#include <string.h>
#include "freertos/FreeRTOS.h"

/*============================== VARIABLES ET CONFIGURATION =======================================*/
/* Bits du registre de controle et format du mot DDS (documentation AD9833). */
#define AD9833_B28                         0x2000U
#define AD9833_RESET                       0x0100U
#define AD9833_ONDE_TRIANGLE_BITS          0x0002U
#define AD9833_ONDE_CARRE_BITS             0x0028U
#define AD9833_FREQUENCE_REGISTRE          0x4000U
#define AD9833_PHASE_REGISTRE_ZERO         0xC000U
#define AD9833_MASQUE_MOT_FREQUENCE        0x3FFFU
#define AD9833_DECALAGE_MOT_HAUT           14U
#define AD9833_COEFFICIENT_DDS             268435456.0 /* 2 puissance 28 */
#define AD9833_SPI_MOT_BITS                16U
#define AD9833_SPI_MOT_OCTETS              2U
#define AD9833_SPI_MODE                    2
#define AD9833_SPI_FILE_TRANSACTIONS       1
#define AD9833_SPI_TIMEOUT_MS              50U
#define AD9833_SPI_MAX_HZ                  40000000U
#define AD9833_HORLOGE_MAX_HZ              25000000U
static spi_host_device_t host;
static ad9833_config_t configuration;
static spi_device_handle_t device;
static spi_transaction_t transaction;
/* Reste valide meme apres un timeout. */
static bool initialise, configure, actif, connu, en_attente, bus_propre;
static uint32_t frequence_horloge_hz;
static uint16_t registre_controle = AD9833_B28 | AD9833_RESET;
static double frequence;
static ad9833_waveform_t forme;
/*==================================== FONCTIONS ================================================*/
static TickType_t delai(void)
{
    TickType_t ticks_attente = pdMS_TO_TICKS(AD9833_SPI_TIMEOUT_MS);
    if (ticks_attente)
    {
        return ticks_attente;
    }
    return 1;
}

static esp_err_t finir_transaction(void)
{
    if (!en_attente)
    {
        return ESP_OK;
    }
    spi_transaction_t *terminee = NULL;
    esp_err_t resultat_spi = spi_device_get_trans_result(device, &terminee, delai());
    if (resultat_spi == ESP_OK)
    {
        en_attente = false;
    }
    return resultat_spi;
}

static esp_err_t ecrire(uint16_t v)
{
    if (!device)
    {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t resultat_spi = finir_transaction();
    if (resultat_spi != ESP_OK)
    {
        return resultat_spi;
    }
    memset(&transaction, 0, sizeof(transaction));
    transaction.flags = SPI_TRANS_USE_TXDATA;
    transaction.length = AD9833_SPI_MOT_BITS;
    transaction.tx_data[0] =(uint8_t)(v >> 8);
    transaction.tx_data[1] =(uint8_t) v;
    resultat_spi = spi_device_queue_trans(device, &transaction, delai());
    if (resultat_spi != ESP_OK)
    {
        return resultat_spi;
    }
    en_attente = true;
    return finir_transaction();
}

static esp_err_t reset(void)
{
    esp_err_t resultat_spi = ecrire(AD9833_B28 | AD9833_RESET);
    connu = resultat_spi == ESP_OK;
    actif = false;
    if (resultat_spi == ESP_OK)
    {
        registre_controle = AD9833_B28 | AD9833_RESET;
    }
    return resultat_spi;
}

static esp_err_t echec(esp_err_t resultat_spi)
{
    configure = false;
    connu = false;
    actif = false;
    (void) reset();
    /* Le code d'erreur initial reste prioritaire. */
    return resultat_spi;
}

/*================================ INTERFACE PUBLIQUE ============================================*/
/*--------------------------------------------------------------------------------------------------
 * Fonction   : Ad9833_Init()
 * Descriptif : Configure le bus SPI et la mise en securite du DDS.
 *--------------------------------------------------------------------------------------------------*/
esp_err_t Ad9833_Init(const ad9833_config_t *c)
{
    if (!c || !GPIO_IS_VALID_OUTPUT_GPIO(c->mosi_gpio) || !GPIO_IS_VALID_OUTPUT_GPIO(c->sclk_gpio) || !GPIO_IS_VALID_OUTPUT_GPIO(c->fsync_gpio)
        || c->mosi_gpio == c->sclk_gpio || c->mosi_gpio == c->fsync_gpio || c->sclk_gpio == c->fsync_gpio || !c->spi_clock_hz
        || c->spi_clock_hz > AD9833_SPI_MAX_HZ || !c->mclk_hz || c->mclk_hz > AD9833_HORLOGE_MAX_HZ)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (initialise)
    {
        bool meme = c->host == configuration.host && c->mosi_gpio == configuration.mosi_gpio && c->sclk_gpio == configuration.sclk_gpio
            && c->fsync_gpio == configuration.fsync_gpio && c->spi_clock_hz == configuration.spi_clock_hz && c->mclk_hz == configuration.mclk_hz;
        if (meme && configure)
        {
            return ESP_OK;
        }
        return ESP_ERR_INVALID_STATE;
    }
    if (device)
    {
        return ESP_ERR_INVALID_STATE;
    }
    spi_bus_config_t configuration_bus_spi = {
        .mosi_io_num = c->mosi_gpio,
        .miso_io_num = -1,
        .sclk_io_num = c->sclk_gpio,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = AD9833_SPI_MOT_OCTETS
    };
    esp_err_t resultat_spi = spi_bus_initialize(c->host, &configuration_bus_spi, SPI_DMA_CH_AUTO);
    if (resultat_spi != ESP_OK)
    {
        return resultat_spi;
    }
    /* Pas de reutilisation d'un bus de brochage inconnu. */
    host = c->host;
    bus_propre = true;
    spi_device_interface_config_t configuration_dispositif_spi = {
        .clock_speed_hz =(int) c->spi_clock_hz,
        .mode = AD9833_SPI_MODE,
        .spics_io_num = c->fsync_gpio,
        .queue_size = AD9833_SPI_FILE_TRANSACTIONS
    };
    resultat_spi = spi_bus_add_device(host, &configuration_dispositif_spi, &device);
    if (resultat_spi != ESP_OK)
    {
        (void) spi_bus_free(host);
        bus_propre = false;
        device = NULL;
        return resultat_spi;
    }
    configuration = *c;
    initialise = true;
    frequence_horloge_hz = c->mclk_hz;
    configure = false;
    connu = false;
    actif = false;
    en_attente = false;
    resultat_spi = Ad9833_SetSignal(AD9833_WAVE_SINE, 0);
    if (resultat_spi != ESP_OK)
    {
        (void) Ad9833_Deinit();
        return resultat_spi;
    }
    return ESP_OK;
}

esp_err_t Ad9833_Deinit(void)
{
    if (!device && !bus_propre)
    {
        return ESP_OK;
    }
    if (device)
    {
        esp_err_t resultat_spi = reset();
        if (resultat_spi != ESP_OK)
        {
            return resultat_spi;
        }
        resultat_spi = spi_bus_remove_device(device);
        if (resultat_spi != ESP_OK)
        {
            return resultat_spi;
        }
        device = NULL;
        initialise = false;
        configure = false;
    }
    if (bus_propre)
    {
        esp_err_t resultat_spi = spi_bus_free(host);
        if (resultat_spi != ESP_OK)
        {
            return resultat_spi;
        }
        bus_propre = false;
    }
    frequence_horloge_hz = 0;
    frequence = 0;
    forme = AD9833_WAVE_SINE;
    return ESP_OK;
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : Ad9833_SetSignal()
 * Descriptif : Charge les registres de frequence et de forme d'onde.
 *--------------------------------------------------------------------------------------------------*/
esp_err_t Ad9833_SetSignal(ad9833_waveform_t f, double hz)
{
    if (!initialise || !device || !frequence_horloge_hz)
    {
        return ESP_ERR_INVALID_STATE;
    }
    if (!isfinite(hz) || hz < 0 || hz >(double) frequence_horloge_hz / 2 || f < AD9833_WAVE_SINE || f > AD9833_WAVE_SQUARE)
    {
        return ESP_ERR_INVALID_ARG;
    }
    uint16_t bits_forme = 0;
    if (f == AD9833_WAVE_TRIANGLE)
    {
        bits_forme = AD9833_ONDE_TRIANGLE_BITS;
    }
    else if (f == AD9833_WAVE_SQUARE)
    {
        bits_forme = AD9833_ONDE_CARRE_BITS;
    }
    uint32_t mot_frequence =(uint32_t)(hz * AD9833_COEFFICIENT_DDS / frequence_horloge_hz + 0.5);
    configure = false;
    esp_err_t resultat_spi = reset();
    if (resultat_spi != ESP_OK)
    {
        return echec(resultat_spi);
    }
    const uint16_t mots_configuration[] = {
        (uint16_t)(AD9833_FREQUENCE_REGISTRE |(mot_frequence & AD9833_MASQUE_MOT_FREQUENCE)),
        (uint16_t)(AD9833_FREQUENCE_REGISTRE |((mot_frequence >> AD9833_DECALAGE_MOT_HAUT) & AD9833_MASQUE_MOT_FREQUENCE)),
        AD9833_PHASE_REGISTRE_ZERO,
        (uint16_t)(AD9833_B28 | AD9833_RESET | bits_forme)
    };
    for (unsigned index_mot = 0; index_mot < sizeof(mots_configuration) / sizeof(mots_configuration[0]); index_mot++)
    {
        resultat_spi = ecrire(mots_configuration[index_mot]);
        if (resultat_spi != ESP_OK)
        {
            return echec(resultat_spi);
        }
    }
    registre_controle = AD9833_B28 | AD9833_RESET | bits_forme;
    frequence = hz;
    forme = f;
    configure = true;
    connu = true;
    actif = false;
    return ESP_OK;
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : Ad9833_EnableOutput()
 * Descriptif : Controle RESET sans promettre une VOUT nulle.
 *--------------------------------------------------------------------------------------------------*/
esp_err_t Ad9833_EnableOutput(bool enable)
{
    if (!initialise || !device ||(enable &&(!configure || !connu)))
    {
        return ESP_ERR_INVALID_STATE;
    }
    uint16_t c;
    if (enable)
    {
        c =(uint16_t)(registre_controle & ~AD9833_RESET);
    }
    else
    {
        c =(uint16_t)(registre_controle | AD9833_RESET);
    }
    esp_err_t resultat_spi = ecrire(c);
    if (resultat_spi != ESP_OK)
    {
        connu = false;
        configure = false;
        actif = false;
        return resultat_spi;
    }
    registre_controle = c;
    connu = true;
    actif = enable;
    return ESP_OK;
}

bool Ad9833_IsInitialized(void)
{
    return initialise;
}

bool Ad9833_IsOutputEnabled(void)
{
    return connu && actif;
}

bool Ad9833_IsOutputStateKnown(void)
{
    return connu;
}

bool Ad9833_IsConfigurationValid(void)
{
    return configure;
}

double Ad9833_GetFrequencyHz(void)
{
    return frequence;
}

ad9833_waveform_t Ad9833_GetWaveform(void)
{
    return forme;
}

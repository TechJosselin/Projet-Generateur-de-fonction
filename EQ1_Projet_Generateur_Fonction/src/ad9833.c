/* CFPT - Base du pilote : travail de Samuel (RDMR_GenerationSignal).
 * Finalisation : PHASE0, entrees finies, transitions explicites et timeout SPI.
 * RESET n'est ni une isolation de VOUT, ni une garantie de tension nulle. */
#include "ad9833.h"
#include <math.h>
#include <stddef.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#define B28 0x2000U
#define RESET 0x0100U
static spi_host_device_t host;
static ad9833_config_t configuration;
static spi_device_handle_t device;
static spi_transaction_t transaction; /* Reste valide meme apres un timeout. */
static bool initialise,configure,actif,connu,en_attente,bus_propre;
static uint32_t mclk;
static uint16_t controle=B28|RESET;
static double frequence;
static ad9833_waveform_t forme;
static TickType_t delai(void){TickType_t t=pdMS_TO_TICKS(50);return t?t:1;}
static esp_err_t finir_transaction(void) {
    if(!en_attente)return ESP_OK;
    spi_transaction_t *terminee=NULL;
    esp_err_t e=spi_device_get_trans_result(device,&terminee,delai());
    if(e==ESP_OK)en_attente=false;
    return e;
}
static esp_err_t ecrire(uint16_t v) {
    if(!device)return ESP_ERR_INVALID_STATE;
    esp_err_t e=finir_transaction();if(e!=ESP_OK)return e;
    memset(&transaction,0,sizeof(transaction));transaction.flags=SPI_TRANS_USE_TXDATA;
    transaction.length=16;transaction.tx_data[0]=(uint8_t)(v>>8);transaction.tx_data[1]=(uint8_t)v;
    e=spi_device_queue_trans(device,&transaction,delai());
    if(e!=ESP_OK)return e;
    en_attente=true;return finir_transaction();
}
static esp_err_t reset(void) {
    esp_err_t e=ecrire(B28|RESET);
    connu=e==ESP_OK;actif=false;
    if(e==ESP_OK)controle=B28|RESET;
    return e;
}
static esp_err_t echec(esp_err_t e) {
    configure=false;connu=false;actif=false;
    (void)reset(); /* Le code d'erreur initial reste prioritaire. */
    return e;
}
esp_err_t Ad9833_Init(const ad9833_config_t *c) {
    if(!c || !GPIO_IS_VALID_OUTPUT_GPIO(c->mosi_gpio) || !GPIO_IS_VALID_OUTPUT_GPIO(c->sclk_gpio) ||
       !GPIO_IS_VALID_OUTPUT_GPIO(c->fsync_gpio) || c->mosi_gpio==c->sclk_gpio ||
       c->mosi_gpio==c->fsync_gpio || c->sclk_gpio==c->fsync_gpio ||
       !c->spi_clock_hz || c->spi_clock_hz>40000000 || !c->mclk_hz || c->mclk_hz>25000000)return ESP_ERR_INVALID_ARG;
    if(initialise) {
        bool meme=c->host==configuration.host && c->mosi_gpio==configuration.mosi_gpio &&
            c->sclk_gpio==configuration.sclk_gpio && c->fsync_gpio==configuration.fsync_gpio &&
            c->spi_clock_hz==configuration.spi_clock_hz && c->mclk_hz==configuration.mclk_hz;
        return meme && configure?ESP_OK:ESP_ERR_INVALID_STATE;
    }
    if(device)return ESP_ERR_INVALID_STATE;
    spi_bus_config_t b={.mosi_io_num=c->mosi_gpio,.miso_io_num=-1,.sclk_io_num=c->sclk_gpio,
        .quadwp_io_num=-1,.quadhd_io_num=-1,.max_transfer_sz=2};
    esp_err_t e=spi_bus_initialize(c->host,&b,SPI_DMA_CH_AUTO);
    if(e!=ESP_OK)return e; /* Pas de reutilisation d'un bus de brochage inconnu. */
    host=c->host;bus_propre=true;
    spi_device_interface_config_t d={.clock_speed_hz=(int)c->spi_clock_hz,.mode=2,.spics_io_num=c->fsync_gpio,.queue_size=1};
    e=spi_bus_add_device(host,&d,&device);
    if(e!=ESP_OK){(void)spi_bus_free(host);bus_propre=false;device=NULL;return e;}
    configuration=*c;initialise=true;mclk=c->mclk_hz;configure=false;connu=false;actif=false;en_attente=false;
    e=Ad9833_SetSignal(AD9833_WAVE_SINE,0);
    if(e!=ESP_OK){(void)Ad9833_Deinit();return e;}
    return ESP_OK;
}
esp_err_t Ad9833_Deinit(void) {
    if(!device && !bus_propre)return ESP_OK;
    if(device) {
        esp_err_t e=reset();if(e!=ESP_OK)return e;
        e=spi_bus_remove_device(device);if(e!=ESP_OK)return e;
        device=NULL;initialise=false;configure=false;
    }
    if(bus_propre) {esp_err_t e=spi_bus_free(host);if(e!=ESP_OK)return e;bus_propre=false;}
    mclk=0;frequence=0;forme=AD9833_WAVE_SINE;return ESP_OK;
}
esp_err_t Ad9833_SetSignal(ad9833_waveform_t f,double hz) {
    if(!initialise || !device || !mclk)return ESP_ERR_INVALID_STATE;
    if(!isfinite(hz) || hz<0 || hz>(double)mclk/2 || f<AD9833_WAVE_SINE || f>AD9833_WAVE_SQUARE)return ESP_ERR_INVALID_ARG;
    uint16_t bits=f==AD9833_WAVE_TRIANGLE?0x0002U:(f==AD9833_WAVE_SQUARE?0x0028U:0U);
    uint32_t mot=(uint32_t)(hz*268435456.0/mclk+0.5);
    configure=false;esp_err_t e=reset();if(e!=ESP_OK)return echec(e);
    const uint16_t mots[]={ (uint16_t)(0x4000U|(mot&0x3FFFU)),
        (uint16_t)(0x4000U|((mot>>14)&0x3FFFU)),0xC000U,(uint16_t)(B28|RESET|bits)};
    for(unsigned i=0;i<sizeof(mots)/sizeof(mots[0]);i++){e=ecrire(mots[i]);if(e!=ESP_OK)return echec(e);}
    controle=B28|RESET|bits;frequence=hz;forme=f;configure=true;connu=true;actif=false;return ESP_OK;
}
esp_err_t Ad9833_EnableOutput(bool enable) {
    if(!initialise || !device || (enable && (!configure || !connu)))return ESP_ERR_INVALID_STATE;
    uint16_t c=enable?(uint16_t)(controle&~RESET):(uint16_t)(controle|RESET);
    esp_err_t e=ecrire(c);
    if(e!=ESP_OK){connu=false;configure=false;actif=false;return e;}
    controle=c;connu=true;actif=enable;return ESP_OK;
}
bool Ad9833_IsInitialized(void){return initialise;}
bool Ad9833_IsOutputEnabled(void){return connu && actif;}
bool Ad9833_IsOutputStateKnown(void){return connu;}
bool Ad9833_IsConfigurationValid(void){return configure;}
double Ad9833_GetFrequencyHz(void){return frequence;}
ad9833_waveform_t Ad9833_GetWaveform(void){return forme;}

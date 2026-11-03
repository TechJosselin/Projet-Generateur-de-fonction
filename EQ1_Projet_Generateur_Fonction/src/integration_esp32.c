#include "integration.h"
#include "ad9833.h"
#include "epot.h"
#include "eeprom.h"
#include "sorties_digitales.h"
#include "i2c_commun.h"
#include "hardware_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static gf_result_t traduire(esp_err_t e) {
    if(e==ESP_OK)return GF_OK;
    if(e==ESP_ERR_TIMEOUT)return GF_TIMEOUT;
    if(e==ESP_ERR_INVALID_RESPONSE)return GF_VERIFICATION;
    if(e==ESP_ERR_INVALID_ARG)return GF_ARGUMENT;
    return GF_IO;
}
static gf_result_t arret(void *ctx){(void)ctx;return traduire(Ad9833_EnableOutput(false));}
static gf_result_t signal(void *ctx,const signal_config_t *s){(void)ctx;return traduire(Ad9833_SetSignal((ad9833_waveform_t)s->forme,s->frequence_hz));}
static gf_result_t gain(void *ctx,uint8_t c){(void)ctx;return traduire(Epot_SetGainRaw(c));}
static gf_result_t offset(void *ctx,uint8_t c){(void)ctx;return traduire(Epot_SetOffsetRaw(c));}
static gf_result_t activer(void *ctx){(void)ctx;return traduire(Ad9833_EnableOutput(true));}
static void attendre(void *ctx,uint16_t ms){(void)ctx;vTaskDelay(pdMS_TO_TICKS(ms)+1);}
static gf_result_t digital(void *ctx,unsigned c,uint32_t hz,uint16_t d,bool on){(void)ctx;return digitales_commander(c,hz,d,on);}
void integration_esp32_initialiser(void) {
    /* DDS sous RESET avant toute lecture de catalogue ou commande EPOT. */
    ad9833_config_t d={SYSTEM_AD9833_SPI_HOST,SYSTEM_AD9833_MOSI_GPIO,
        SYSTEM_AD9833_SCLK_GPIO,SYSTEM_AD9833_FSYNC_GPIO,SYSTEM_AD9833_SPI_CLOCK_HZ,SYSTEM_AD9833_MCLK_HZ};
    integration_backend_t b={.arreter=arret,.dds_configurer=signal,.gain=gain,.offset=offset,
        .activer=activer,.attendre_ms=attendre,.digital=digital};
    b.dds=Ad9833_Init(&d)==ESP_OK;
    b.i2c=i2c_commun_initialiser()==ESP_OK;
    if(b.i2c) {
        b.epots=Epot_Init(i2c_commun_bus(),SYSTEM_EPOT_GAIN_I2C_ADDRESS,SYSTEM_EPOT_OFFSET_I2C_ADDRESS)==ESP_OK;
        b.eeprom=eeprom_initialiser(i2c_commun_bus(),SYSTEM_EEPROM_I2C_ADDRESS)==ESP_OK;
    }
    digitales_initialiser();for(unsigned c=0;c<2;c++)b.digitales[c]=digitales_disponible(c);
    integration_initialiser_avec(b,eeprom_interface());
}

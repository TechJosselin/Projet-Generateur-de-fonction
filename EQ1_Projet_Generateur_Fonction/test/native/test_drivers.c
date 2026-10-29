#include "ad9833.h"
#include "epot.h"
#include "eeprom.h"
#include "oled.h"
#include "sorties_digitales.h"
#include "driver/ledc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static unsigned checks;
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"ECHEC %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static int spi_calls,spi_fail,spi_pending_timeout;
static unsigned word_count;
static uint16_t words[128];
static spi_transaction_t *pending;
esp_err_t spi_bus_initialize(int h,const spi_bus_config_t *c,int dma){(void)h;(void)dma;CHECK(c->miso_io_num==-1 && c->max_transfer_sz==2);return ESP_OK;}
esp_err_t spi_bus_free(int h){(void)h;return ESP_OK;}
esp_err_t spi_bus_add_device(int h,const spi_device_interface_config_t *c,spi_device_handle_t *d){(void)h;CHECK(c->mode==2 && c->queue_size==1);*d=(void*)1;return ESP_OK;}
esp_err_t spi_bus_remove_device(spi_device_handle_t d){(void)d;CHECK(!pending);return ESP_OK;}
esp_err_t spi_device_queue_trans(spi_device_handle_t d,spi_transaction_t *t,unsigned ticks){
    (void)d;CHECK(ticks>0 && ticks<100 && !pending);spi_calls++;
    if(spi_calls==spi_fail)return ESP_FAIL;
    CHECK(t->length==16 && (t->flags&SPI_TRANS_USE_TXDATA));pending=t;
    if(word_count<128)words[word_count++]=(uint16_t)((t->tx_data[0]<<8)|t->tx_data[1]);
    return ESP_OK;
}
esp_err_t spi_device_get_trans_result(spi_device_handle_t d,spi_transaction_t **t,unsigned ticks){
    (void)d;CHECK(ticks>0 && pending);
    if(spi_pending_timeout)return ESP_ERR_TIMEOUT;
    *t=pending;pending=NULL;return ESP_OK;
}
struct fake_device {unsigned a;bool allocated;uint8_t reg[16];};
static struct fake_device devs[128];
static uint8_t memory[4096],oled_page[128];
static unsigned active_handles,adds,page_transfers;
static int i2c_calls,i2c_fail;
static bool read_wrong,absent;
static bool i2c_ok(void){i2c_calls++;return !absent && (!i2c_fail || i2c_calls!=i2c_fail);}
esp_err_t i2c_master_probe(i2c_master_bus_handle_t b,uint16_t a,int ms){(void)b;(void)a;CHECK(ms>0 && ms<=50);return i2c_ok()?ESP_OK:ESP_ERR_NOT_FOUND;}
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t b,const i2c_device_config_t *c,i2c_master_dev_handle_t *d){
    (void)b;CHECK(c->scl_speed_hz==100000 && c->device_address<128);if(!i2c_ok())return ESP_FAIL;
    struct fake_device *p=&devs[c->device_address];CHECK(!p->allocated);p->a=c->device_address;p->allocated=true;*d=p;active_handles++;adds++;return ESP_OK;
}
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t d){CHECK(d && d->allocated);d->allocated=false;active_handles--;return ESP_OK;}
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t d,const uint8_t *b,size_t n,int ms){
    CHECK(d && d->allocated && ms>0 && ms<=50);if(!i2c_ok())return ESP_FAIL;
    if(d->a==0x50){CHECK(n>=3 && n<=34);unsigned a=((unsigned)b[0]<<8)|b[1];CHECK(a+n-2<=4096 && a/32==(a+n-3)/32);memcpy(memory+a,b+2,n-2);}
    else if(d->a==0x3F || d->a==0x3E){CHECK(n==2 && (b[0]&15)==0);d->reg[b[0]>>4]=b[1];}
    else if(n==129){CHECK(b[0]==0x40);memcpy(oled_page,b+1,128);page_transfers++;}
    return ESP_OK;
}
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t d,const uint8_t *b,size_t n,uint8_t *out,size_t m,int ms){
    CHECK(d && d->allocated && ms>0 && ms<=50);if(!i2c_ok())return ESP_FAIL;
    if(d->a==0x50){CHECK(n==2 && m<=128);unsigned a=((unsigned)b[0]<<8)|b[1];CHECK(a+m<=4096);memcpy(out,memory+a,m);}
    else {CHECK(n==1 && m==2 && (b[0]&15)==12);out[0]=0;out[1]=read_wrong?0:d->reg[b[0]>>4];}
    return ESP_OK;
}
esp_err_t i2c_commun_initialiser(void){return ESP_OK;}
i2c_master_bus_handle_t i2c_commun_bus(void){return (void*)1;}
static uint32_t pwm_hz[2];static int levels[31];static bool pwm_fail;
esp_err_t gpio_set_level(int p,int l){CHECK(p>=0 && p<31);levels[p]=l;return ESP_OK;}
esp_err_t gpio_config(const gpio_config_t *c){CHECK(c->pin_bit_mask);return ESP_OK;}
esp_err_t ledc_timer_config(const ledc_timer_config_t *c){CHECK(c->timer_num<2 && c->clk_cfg==LEDC_USE_XTAL_CLK);pwm_hz[c->timer_num]=c->freq_hz;return pwm_fail?ESP_FAIL:ESP_OK;}
uint32_t ledc_get_freq(int mode,int timer){(void)mode;return pwm_hz[timer];}
esp_err_t ledc_channel_config(const ledc_channel_config_t *c){CHECK(c->timer_sel==c->channel);return ESP_OK;}
esp_err_t ledc_stop(int mode,int channel,int level){(void)mode;CHECK(channel<2 && level<=1);return ESP_OK;}
static void dds(void) {
    ad9833_config_t cfg={SPI2_HOST,18,19,21,1000000,25000000};
    CHECK(Ad9833_Init(&cfg)==ESP_OK);CHECK(word_count==5);CHECK(words[3]==0xC000 && words[4]==0x2100);
    word_count=0;CHECK(Ad9833_SetSignal(AD9833_WAVE_SINE,10000)==ESP_OK);
    CHECK(words[0]==0x2100 && words[1]==0x636E && words[2]==0x4006 && words[3]==0xC000 && words[4]==0x2100);
    int avant=spi_calls;CHECK(Ad9833_SetSignal(AD9833_WAVE_SINE,NAN)==ESP_ERR_INVALID_ARG);
    CHECK(Ad9833_SetSignal(AD9833_WAVE_SINE,INFINITY)==ESP_ERR_INVALID_ARG);
    CHECK(Ad9833_SetSignal(AD9833_WAVE_SINE,-1)==ESP_ERR_INVALID_ARG);
    CHECK(Ad9833_SetSignal(AD9833_WAVE_SINE,12500001)==ESP_ERR_INVALID_ARG);CHECK(spi_calls==avant);
    CHECK(Ad9833_EnableOutput(true)==ESP_OK);CHECK(Ad9833_IsOutputEnabled());
    CHECK(Ad9833_SetSignal(AD9833_WAVE_TRIANGLE,1000)==ESP_OK);CHECK(!Ad9833_IsOutputEnabled());
    CHECK(Ad9833_SetSignal(AD9833_WAVE_SQUARE,1000)==ESP_OK);CHECK(Ad9833_EnableOutput(true)==ESP_OK);CHECK(words[word_count-1]==0x2028);
    for(int f=1;f<=5;f++) {
        spi_fail=spi_calls+f;CHECK(Ad9833_SetSignal(AD9833_WAVE_SINE,1234)!=ESP_OK);
        CHECK(!Ad9833_IsConfigurationValid() && !Ad9833_IsOutputEnabled());
        CHECK(Ad9833_EnableOutput(true)==ESP_ERR_INVALID_STATE);
        spi_fail=0;CHECK(Ad9833_SetSignal(AD9833_WAVE_SINE,1000)==ESP_OK);
    }
    spi_pending_timeout=1;CHECK(Ad9833_SetSignal(AD9833_WAVE_SINE,2000)==ESP_ERR_TIMEOUT);
    CHECK(pending && !Ad9833_IsOutputStateKnown());
    uint8_t saved[4];memcpy(saved,pending->tx_data,4);
    CHECK(Ad9833_Deinit()==ESP_ERR_TIMEOUT);CHECK(!memcmp(saved,pending->tx_data,4));
    spi_pending_timeout=0;CHECK(Ad9833_EnableOutput(false)==ESP_OK);CHECK(!pending);CHECK(Ad9833_Deinit()==ESP_OK);
    puts("PASS AD9833 : mots SPI, PHASE0, NaN, 5 positions d'echec, timeout et duree de vie transaction.");
}
static void epot(void) {
    for(int f=1;f<=8;f++) {
        i2c_calls=0;i2c_fail=f;CHECK(Epot_Init((void*)1,0x3F,0x3E)!=ESP_OK);CHECK(active_handles==0);
    }
    i2c_fail=0;CHECK(Epot_Init((void*)1,0x3F,0x3E)==ESP_OK);CHECK(active_handles==2);
    uint8_t v=0;CHECK(Epot_GetLastRaw(EPOT_CHANNEL_GAIN,&v)==ESP_ERR_INVALID_STATE);
    CHECK(Epot_SetGainRaw(255)==ESP_OK);CHECK(Epot_GetLastRaw(EPOT_CHANNEL_GAIN,&v)==ESP_OK && v==255);
    CHECK(Epot_SetOffsetRaw(0)==ESP_OK);CHECK(Epot_ReadRaw(EPOT_CHANNEL_OFFSET,&v)==ESP_OK && v==0);
    for(int f=1;f<=2;f++) {i2c_fail=i2c_calls+f;CHECK(Epot_SetGainRaw(100)!=ESP_OK);CHECK(Epot_GetLastRaw(EPOT_CHANNEL_GAIN,&v)==ESP_ERR_INVALID_STATE);i2c_fail=0;}
    read_wrong=true;CHECK(Epot_SetGainRaw(100)==ESP_ERR_INVALID_RESPONSE);read_wrong=false;
    CHECK(Epot_Deinit()==ESP_OK && active_handles==0);puts("PASS EPOT : 8 echecs init, ecriture/relecture, TCON, valeur relue et nettoyage.");
}
static void eeprom(void) {
    CHECK(eeprom_initialiser((void*)1,0x50)==ESP_OK);stockage_io_t e=eeprom_interface();
    uint8_t b[256],out[256];for(unsigned i=0;i<sizeof(b);i++)b[i]=(uint8_t)i;
    CHECK(e.page_write(e.ctx,31,b,2)==GF_ARGUMENT);CHECK(e.page_write(e.ctx,4096,b,1)==GF_ARGUMENT);
    for(unsigned i=0;i<256;i+=32)CHECK(e.page_write(e.ctx,(uint16_t)i,b+i,32)==GF_OK);
    CHECK(e.read(e.ctx,0,out,256)==GF_OK && !memcmp(b,out,256));CHECK(e.read(e.ctx,4090,out,20)==GF_ARGUMENT);
    absent=true;CHECK(e.ready(e.ctx)==GF_OCCUPE);CHECK(e.read(e.ctx,0,out,10)==GF_IO);absent=false;
    puts("PASS pilote EEPROM : adresses 16 bits, pages, lecture fractionnee et NACK.");
}
static void digital(void) {
    digitales_initialiser();CHECK(digitales_disponible(0) && digitales_disponible(1));
    CHECK(digitales_commander(0,1000,500,true)==GF_OK);CHECK(digitales_commander(1,100000,250,true)==GF_OK);
    CHECK(pwm_hz[0]==1000 && pwm_hz[1]==100000);CHECK(digitales_commander(0,1,500,true)==GF_PLAGE);
    CHECK(digitales_commander(0,1000,1000,true)==GF_OK);CHECK(digitales_commander(1,0,0,false)==GF_OK);
    pwm_fail=true;CHECK(digitales_commander(0,2000,500,true)==GF_IO);pwm_fail=false;
    puts("PASS PWM optionnel : deux timers, bornes, niveau fixe et erreur de pilote. GPIO de test uniquement.");
}
int main(void){dds();epot();eeprom();digital();printf("RESULTAT pilotes : %u assertions. Aucune carte physique.\n",checks);return 0;}

#include "sorties_digitales.h"
#include "hardware_config.h"
#include "board_options.h"
#include "driver/ledc.h"
static const int pins[2]={SYSTEM_DIGITAL_1_GPIO,SYSTEM_DIGITAL_2_GPIO};
static bool disponibles[2],configures[2];
static bool pin_autorise(int p) {
    if(!GPIO_IS_VALID_OUTPUT_GPIO(p))return false;
    /* USB, flash, straps, LED et autres fonctions existantes restent reserves. */
    const int reserves[]={0,1,2,4,5,8,9,12,13,15,18,19,21,22,23,24,25,26,27,28,29,30};
    for(unsigned i=0;i<sizeof(reserves)/sizeof(reserves[0]);i++)if(p==reserves[i])return false;
    return true;
}
void digitales_initialiser(void) {
    for(unsigned c=0;c<2;c++) {
        disponibles[c]=false;configures[c]=false;
        if(pins[0]==pins[1] || !pin_autorise(pins[c]))continue;
        gpio_config_t g={.pin_bit_mask=1ULL<<pins[c],.mode=GPIO_MODE_OUTPUT,
            .pull_up_en=GPIO_PULLUP_DISABLE,.pull_down_en=GPIO_PULLDOWN_DISABLE,.intr_type=GPIO_INTR_DISABLE};
        if(gpio_set_level(pins[c],0)==ESP_OK && gpio_config(&g)==ESP_OK)disponibles[c]=true;
    }
}
bool digitales_disponible(unsigned c){return c<2 && disponibles[c];}
gf_result_t digitales_commander(unsigned c,uint32_t hz,uint16_t duty,bool on) {
    if(c>=2 || duty>1000)return GF_ARGUMENT;
    if(!disponibles[c])return GF_NON_SUPPORTE;
    if(!on) {
        esp_err_t e=configures[c]?ledc_stop(LEDC_LOW_SPEED_MODE,(ledc_channel_t)c,0):gpio_set_level(pins[c],0);
        return e==ESP_OK?GF_OK:GF_IO;
    }
    if(hz<10 || hz>1000000)return GF_PLAGE;
    unsigned bits=14;
    while(bits>1 && ((uint64_t)hz<<bits)>40000000U)bits--;
    ledc_timer_config_t t={.speed_mode=LEDC_LOW_SPEED_MODE,.timer_num=(ledc_timer_t)c,
        .duty_resolution=(ledc_timer_bit_t)bits,.freq_hz=hz,.clk_cfg=LEDC_USE_XTAL_CLK};
    esp_err_t e=ledc_timer_config(&t);
    if(e!=ESP_OK)goto fail;
    uint32_t real=ledc_get_freq(LEDC_LOW_SPEED_MODE,(ledc_timer_t)c);
    uint32_t diff=real>hz?real-hz:hz-real;
    if(!real || (uint64_t)diff*100U>hz){e=ESP_ERR_INVALID_ARG;goto fail;}
    ledc_channel_config_t ch={.gpio_num=pins[c],.speed_mode=LEDC_LOW_SPEED_MODE,
        .channel=(ledc_channel_t)c,.timer_sel=(ledc_timer_t)c,.intr_type=LEDC_INTR_DISABLE,
        .duty=(duty==1000)?0:(uint32_t)((((uint64_t)1<<bits)*duty+500)/1000),.hpoint=0};
    /* Arrondir une consigne presque pleine sans depasser le compteur. */
    if(ch.duty>=(1U<<bits))ch.duty=(1U<<bits)-1;
    e=ledc_channel_config(&ch);if(e!=ESP_OK)goto fail;
    configures[c]=true;
    if(duty==0 || duty==1000)e=ledc_stop(LEDC_LOW_SPEED_MODE,(ledc_channel_t)c,duty==1000);
    if(e==ESP_OK)return GF_OK;
fail:
    if(configures[c])(void)ledc_stop(LEDC_LOW_SPEED_MODE,(ledc_channel_t)c,0);
    return e==ESP_ERR_INVALID_ARG?GF_PLAGE:GF_IO;
}

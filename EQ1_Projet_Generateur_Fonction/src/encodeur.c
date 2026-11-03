/* Decodeur quadrature du prototype, extrait et teste dans entree.c. */
#include "encodeur.h"
#include "hardware_config.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_timer.h"
static QueueHandle_t queue;
static quadrature_t q;
static bouton_t b;
static int attente;
static uint32_t pertes,vues;
static bool initialise;
static portMUX_TYPE verrou=portMUX_INITIALIZER_UNLOCKED;
static uint8_t ab(void){return (uint8_t)((gpio_get_level(SYSTEM_ENCODER_A_GPIO)<<1)|gpio_get_level(SYSTEM_ENCODER_B_GPIO));}
static void interruption(void *unused) {
    (void)unused;uint8_t x=ab();BaseType_t reveil=pdFALSE;
    if(xQueueSendFromISR(queue,&x,&reveil)!=pdTRUE){portENTER_CRITICAL_ISR(&verrou);pertes++;portEXIT_CRITICAL_ISR(&verrou);}
    if(reveil)portYIELD_FROM_ISR();
}
uint32_t encodeur_pertes(void){portENTER_CRITICAL(&verrou);uint32_t v=pertes;portEXIT_CRITICAL(&verrou);return v;}
bool encodeur_initialiser(void) {
    if(initialise)return true;
    queue=xQueueCreate(128,sizeof(uint8_t));if(!queue)return false;
    gpio_config_t g={.pin_bit_mask=(1ULL<<SYSTEM_ENCODER_A_GPIO)|(1ULL<<SYSTEM_ENCODER_B_GPIO),
        .mode=GPIO_MODE_INPUT,.pull_up_en=GPIO_PULLUP_ENABLE,.pull_down_en=GPIO_PULLDOWN_DISABLE,.intr_type=GPIO_INTR_ANYEDGE};
    esp_err_t e=gpio_config(&g);if(e!=ESP_OK)goto fail;
    g.pin_bit_mask=1ULL<<SYSTEM_ENCODER_BUTTON_GPIO;g.intr_type=GPIO_INTR_DISABLE;
    e=gpio_config(&g);if(e!=ESP_OK)goto fail;
    quadrature_initialiser(&q,ab());bouton_initialiser(&b,gpio_get_level(SYSTEM_ENCODER_BUTTON_GPIO)==0,(uint32_t)(esp_timer_get_time()/1000));
    e=gpio_install_isr_service(0);if(e!=ESP_OK && e!=ESP_ERR_INVALID_STATE)goto fail;
    e=gpio_isr_handler_add(SYSTEM_ENCODER_A_GPIO,interruption,NULL);if(e!=ESP_OK)goto fail;
    e=gpio_isr_handler_add(SYSTEM_ENCODER_B_GPIO,interruption,NULL);
    if(e!=ESP_OK){(void)gpio_isr_handler_remove(SYSTEM_ENCODER_A_GPIO);goto fail;}
    initialise=true;return true;
fail:
    vQueueDelete(queue);queue=NULL;return false;
}
evenement_ui_t encodeur_lire_evenement(void) {
    if(!initialise)return UI_AUCUN;
    uint32_t n=encodeur_pertes();
    if(n!=vues){vues=n;xQueueReset(queue);quadrature_initialiser(&q,ab());attente=0;}
    uint8_t x;unsigned traite=0;
    while(traite++<128 && xQueueReceive(queue,&x,0)==pdTRUE) {
        int d=quadrature_traiter(&q,x);
        if(SYSTEM_ENCODER_REVERSE_DIRECTION)d=-d;
        attente+=d;if(attente>128)attente=128;if(attente< -128)attente=-128;
    }
    evenement_ui_t e=bouton_traiter(&b,gpio_get_level(SYSTEM_ENCODER_BUTTON_GPIO)==0,(uint32_t)(esp_timer_get_time()/1000));
    if(e!=UI_AUCUN)return e;
    if(attente>0){attente--;return UI_ROTATION_PLUS;}
    if(attente<0){attente++;return UI_ROTATION_MOINS;}
    return UI_AUCUN;
}

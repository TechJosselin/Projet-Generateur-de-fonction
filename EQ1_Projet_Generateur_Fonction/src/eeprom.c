/* 24LC32A : adresse memoire 16 bits, 4096 octets, pages de 32 octets.
 * Le cycle d'ecriture est suivi par ACK polling dans stockage_pas(), sans spin. */
#include "eeprom.h"
#include <string.h>
static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t device;
static uint8_t adresse;
static gf_result_t convertir(esp_err_t e) {
    return e==ESP_OK?GF_OK:(e==ESP_ERR_TIMEOUT?GF_TIMEOUT:GF_IO);
}
esp_err_t eeprom_initialiser(i2c_master_bus_handle_t b,uint8_t a) {
    if(!b || a<0x50 || a>0x57)return ESP_ERR_INVALID_ARG;
    if(device)return b==bus && a==adresse?ESP_OK:ESP_ERR_INVALID_STATE;
    esp_err_t e=i2c_master_probe(b,a,30);if(e!=ESP_OK)return e;
    i2c_device_config_t cfg={.dev_addr_length=I2C_ADDR_BIT_LEN_7,.device_address=a,.scl_speed_hz=100000};
    e=i2c_master_bus_add_device(b,&cfg,&device);
    if(e==ESP_OK){bus=b;adresse=a;}return e;
}
static gf_result_t lecture(void *ctx,uint16_t a,uint8_t *dst,size_t n) {
    (void)ctx;if(!device)return GF_ABSENT;
    if(!dst || a>4096 || n>(size_t)(4096-a))return GF_ARGUMENT;
    while(n) {
        size_t taille=n>128?128:n;uint8_t cmd[2]={(uint8_t)(a>>8),(uint8_t)a};
        esp_err_t e=i2c_master_transmit_receive(device,cmd,2,dst,taille,30);
        if(e!=ESP_OK)return convertir(e);
        a=(uint16_t)(a+taille);dst+=taille;n-=taille;
    }return GF_OK;
}
static gf_result_t ecriture_page(void *ctx,uint16_t a,const uint8_t *src,size_t n) {
    (void)ctx;if(!device)return GF_ABSENT;
    if(!src || !n || n>32 || a>=4096 || n>(size_t)(4096-a) || a/32!=(a+n-1)/32)return GF_ARGUMENT;
    uint8_t trame[34]={(uint8_t)(a>>8),(uint8_t)a};memcpy(trame+2,src,n);
    return convertir(i2c_master_transmit(device,trame,n+2,30));
}
static gf_result_t pret(void *ctx) {
    (void)ctx;if(!device)return GF_ABSENT;
    esp_err_t e=i2c_master_probe(bus,adresse,20);
    if(e==ESP_ERR_NOT_FOUND)return GF_OCCUPE;
    return convertir(e);
}
stockage_io_t eeprom_interface(void){return (stockage_io_t){NULL,lecture,ecriture_page,pret};}

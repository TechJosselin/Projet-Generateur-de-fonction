/* CFPT - MCP45HV51. Protocoles conserves depuis le pilote du projet.
 * DS20005304B sections 4.4.1.2 et 7.5 : lecture deux octets, donnee dans le LSB.
 * WLAT doit autoriser le transfert et SHDN etre inactif : a verifier sur la carte. */
#include "epot.h"
#include <stddef.h>
static struct {i2c_master_dev_handle_t handle;uint8_t last;bool valide;} devices[2];
static bool initialise;
static i2c_master_bus_handle_t bus_initial;
static uint8_t adresses[2];
static bool canal_valide(epot_channel_t c){return (unsigned)c<2;}
static esp_err_t lire(i2c_master_dev_handle_t d,uint8_t reg,uint8_t *v) {
    uint8_t cmd=(uint8_t)((reg<<4)|0x0CU),data[2];
    esp_err_t e=i2c_master_transmit_receive(d,&cmd,1,data,2,EPOT_I2C_TIMEOUT_MS);
    if(e!=ESP_OK)return e;
    if(data[0]!=0)return ESP_ERR_INVALID_RESPONSE;
    *v=data[1];return ESP_OK;
}
static esp_err_t ecrire_verifier(i2c_master_dev_handle_t d,uint8_t reg,uint8_t v) {
    uint8_t b[2]={(uint8_t)(reg<<4),v},lu=0;
    esp_err_t e=i2c_master_transmit(d,b,sizeof(b),EPOT_I2C_TIMEOUT_MS);
    if(e!=ESP_OK)return e;
    e=lire(d,reg,&lu);return e!=ESP_OK?e:(lu==v?ESP_OK:ESP_ERR_INVALID_RESPONSE);
}
esp_err_t Epot_Deinit(void) {
    esp_err_t premier=ESP_OK;initialise=false;
    for(unsigned c=0;c<2;c++) {
        devices[c].valide=false;
        if(devices[c].handle) {
            esp_err_t e=i2c_master_bus_rm_device(devices[c].handle);
            if(e==ESP_OK)devices[c].handle=NULL;else if(premier==ESP_OK)premier=e;
        }
    }return premier;
}
esp_err_t Epot_Init(i2c_master_bus_handle_t bus,uint8_t ga,uint8_t oa) {
    if(!bus || ga<0x3C || ga>0x3F || oa<0x3C || oa>0x3F || ga==oa)return ESP_ERR_INVALID_ARG;
    if(initialise)return bus==bus_initial && ga==adresses[0] && oa==adresses[1]?ESP_OK:ESP_ERR_INVALID_STATE;
    if(devices[0].handle || devices[1].handle){esp_err_t e=Epot_Deinit();if(e!=ESP_OK)return e;}
    const uint8_t addr[2]={ga,oa};esp_err_t e;
    for(unsigned c=0;c<2;c++) {
        e=i2c_master_probe(bus,addr[c],EPOT_I2C_TIMEOUT_MS);if(e!=ESP_OK)goto fail;
        i2c_device_config_t cfg={.dev_addr_length=I2C_ADDR_BIT_LEN_7,.device_address=addr[c],.scl_speed_hz=EPOT_I2C_SPEED_HZ};
        e=i2c_master_bus_add_device(bus,&cfg,&devices[c].handle);if(e!=ESP_OK)goto fail;
        e=ecrire_verifier(devices[c].handle,4,0xFF);if(e!=ESP_OK)goto fail;
        devices[c].valide=false;
    }
    bus_initial=bus;adresses[0]=ga;adresses[1]=oa;initialise=true;return ESP_OK;
fail:
    (void)Epot_Deinit();return e;
}
esp_err_t Epot_ReadRaw(epot_channel_t c,uint8_t *v) {
    if(!canal_valide(c) || !v)return ESP_ERR_INVALID_ARG;
    if(!initialise || !devices[c].handle)return ESP_ERR_INVALID_STATE;
    esp_err_t e=lire(devices[c].handle,0,v);
    if(e!=ESP_OK)devices[c].valide=false;
    return e;
}
esp_err_t Epot_SetRaw(epot_channel_t c,uint8_t v) {
    if(!canal_valide(c))return ESP_ERR_INVALID_ARG;
    if(!initialise || !devices[c].handle)return ESP_ERR_INVALID_STATE;
    devices[c].valide=false;esp_err_t e=ecrire_verifier(devices[c].handle,0,v);
    if(e==ESP_OK){devices[c].last=v;devices[c].valide=true;}return e;
}
esp_err_t Epot_SetGainRaw(uint8_t v){return Epot_SetRaw(EPOT_CHANNEL_GAIN,v);}
esp_err_t Epot_SetOffsetRaw(uint8_t v){return Epot_SetRaw(EPOT_CHANNEL_OFFSET,v);}
esp_err_t Epot_GetLastRaw(epot_channel_t c,uint8_t *v) {
    if(!canal_valide(c) || !v)return ESP_ERR_INVALID_ARG;
    if(!initialise || !devices[c].valide)return ESP_ERR_INVALID_STATE;
    *v=devices[c].last;return ESP_OK;
}
bool Epot_IsInitialized(void){return initialise;}
bool Epot_IsChannelAvailable(epot_channel_t c){return initialise && canal_valide(c) && devices[c].handle;}

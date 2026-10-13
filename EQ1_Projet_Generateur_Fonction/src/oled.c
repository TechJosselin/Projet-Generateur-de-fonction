#include "oled.h"
#include "i2c_commun.h"
#include "hardware_config.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

/* Brochage preserve depuis le prototype teste sur breadboard. */
#define OLED_LARGEUR 128
#define OLED_PAGES 8

static uint8_t image[OLED_PAGES][OLED_LARGEUR];
static bool present;
static uint8_t adresse;
static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t device;

/* Une petite police 5 x 7, colonnes de bits, non accentuee. */
typedef struct { char c; uint8_t x[5]; } glyph_t;
static const glyph_t police[] = {
    {' ', {0,0,0,0,0}},
    {'!', {0,0,0x5f,0,0}},
    {'+', {0x08,0x08,0x3e,0x08,0x08}},
    {',', {0,0x50,0x30,0,0}},
    {'-', {0x08,0x08,0x08,0x08,0x08}},
    {'.', {0,0x60,0x60,0,0}},
    {'/', {0x20,0x10,0x08,0x04,0x02}},
    {'0', {0x3e,0x51,0x49,0x45,0x3e}},
    {'1', {0,0x42,0x7f,0x40,0}},
    {'2', {0x42,0x61,0x51,0x49,0x46}},
    {'3', {0x21,0x41,0x45,0x4b,0x31}},
    {'4', {0x18,0x14,0x12,0x7f,0x10}},
    {'5', {0x27,0x45,0x45,0x45,0x39}},
    {'6', {0x3c,0x4a,0x49,0x49,0x30}},
    {'7', {0x01,0x71,0x09,0x05,0x03}},
    {'8', {0x36,0x49,0x49,0x49,0x36}},
    {'9', {0x06,0x49,0x49,0x29,0x1e}},
    {':', {0,0x36,0x36,0,0}},
    {'=', {0x14,0x14,0x14,0x14,0x14}},
    {'>', {0x41,0x22,0x14,0x08,0}},
    {'?', {0x02,0x01,0x51,0x09,0x06}},
    {'A', {0x7e,0x11,0x11,0x11,0x7e}},
    {'B', {0x7f,0x49,0x49,0x49,0x36}},
    {'C', {0x3e,0x41,0x41,0x41,0x22}},
    {'D', {0x7f,0x41,0x41,0x22,0x1c}},
    {'E', {0x7f,0x49,0x49,0x49,0x41}},
    {'F', {0x7f,0x09,0x09,0x09,0x01}},
    {'G', {0x3e,0x41,0x49,0x49,0x7a}},
    {'H', {0x7f,0x08,0x08,0x08,0x7f}},
    {'I', {0,0x41,0x7f,0x41,0}},
    {'J', {0x20,0x40,0x41,0x3f,0x01}},
    {'K', {0x7f,0x08,0x14,0x22,0x41}},
    {'L', {0x7f,0x40,0x40,0x40,0x40}},
    {'M', {0x7f,0x02,0x0c,0x02,0x7f}},
    {'N', {0x7f,0x04,0x08,0x10,0x7f}},
    {'O', {0x3e,0x41,0x41,0x41,0x3e}},
    {'P', {0x7f,0x09,0x09,0x09,0x06}},
    {'Q', {0x3e,0x41,0x51,0x21,0x5e}},
    {'R', {0x7f,0x09,0x19,0x29,0x46}},
    {'S', {0x46,0x49,0x49,0x49,0x31}},
    {'T', {0x01,0x01,0x7f,0x01,0x01}},
    {'U', {0x3f,0x40,0x40,0x40,0x3f}},
    {'V', {0x1f,0x20,0x40,0x20,0x1f}},
    {'W', {0x7f,0x20,0x18,0x20,0x7f}},
    {'X', {0x63,0x14,0x08,0x14,0x63}},
    {'Y', {0x03,0x04,0x78,0x04,0x03}},
    {'Z', {0x61,0x51,0x49,0x45,0x43}},
    {'*', {0x14,0x08,0x3e,0x08,0x14}}
};

static const uint8_t *chercher_glyphe(char c)
{
    if(c>='a' && c<='z') c=(char)(c-32);
    for(size_t i=0;i<sizeof(police)/sizeof(police[0]);i++)
        if(police[i].c==c) return police[i].x;
    return police[0].x;
}
static esp_err_t commande(const uint8_t *commandes,size_t n)
{
    if(!present || n>31) return ESP_FAIL;
    uint8_t buf[32];
    buf[0]=0x00;
    memcpy(buf+1,commandes,n);
    return i2c_master_transmit(device,buf,n+1,1000);
}
bool oled_initialiser(void)
{
    esp_err_t err=i2c_commun_initialiser();
    bus=i2c_commun_bus();
    if(err!=ESP_OK){printf("OLED: bus I2C erreur %s\n",esp_err_to_name(err));return false;}
    if(i2c_master_probe(bus,SYSTEM_OLED_I2C_ADDRESS_PRIMARY,SYSTEM_OLED_PROBE_TIMEOUT_MS)==ESP_OK)
        adresse=SYSTEM_OLED_I2C_ADDRESS_PRIMARY;
    else if(i2c_master_probe(bus,SYSTEM_OLED_I2C_ADDRESS_SECONDARY,SYSTEM_OLED_PROBE_TIMEOUT_MS)==ESP_OK)
        adresse=SYSTEM_OLED_I2C_ADDRESS_SECONDARY;
    else {printf("OLED non detecte: verifier les connexions.\n");return false;}
    i2c_device_config_t cdev={
        .dev_addr_length=I2C_ADDR_BIT_LEN_7,
        .device_address=adresse,
        .scl_speed_hz=SYSTEM_OLED_I2C_SPEED_HZ
    };
    err=i2c_master_bus_add_device(bus,&cdev,&device);
    if(err!=ESP_OK){printf("OLED: device erreur %s\n",esp_err_to_name(err));return false;}
    present=true;
    static const uint8_t commandes[]={
        0xAE, 0xD5,0x80, 0xA8,0x3F, 0xD3,0x00, 0x40,
        0xAD,0x8B, 0xA1,0xC8, 0xDA,0x12, 0x81,0x80,
        0xD9,0x22, 0xDB,0x35, 0xA4,0xA6,0xAF
    };
    if(commande(commandes,sizeof(commandes))!=ESP_OK){present=false;return false;}
    printf("OLED SH1106: I2C 0x%02X OK.\n",adresse);
    return true;
}
bool oled_est_present(void){return present;}
uint8_t oled_adresse(void){return adresse;}
void oled_effacer(void){memset(image,0,sizeof(image));}
void oled_texte(int ligne,int colonne,const char *texte)
{
    if(ligne<0 || ligne>=OLED_PAGES || colonne<0 || !texte) return;
    int x=colonne*6;
    while(*texte && x+5<OLED_LARGEUR){
        const uint8_t *g=chercher_glyphe(*texte++);
        for(int i=0;i<5;i++) image[ligne][x+i]=g[i];
        image[ligne][x+5]=0;
        x+=6;
    }
}
void oled_ligne(int ligne,const char *texte,bool surligner)
{
    if(ligne<0 || ligne>=OLED_PAGES) return;
    oled_texte(ligne,0,texte);
    if(surligner) for(int x=0;x<OLED_LARGEUR;x++) image[ligne][x]^=0xFF;
}
void oled_pixel(int x,int y)
{
    if(x>=0 && x<OLED_LARGEUR && y>=0 && y<OLED_PAGES*8)
        image[y/8][x]|=(uint8_t)(1U<<(y%8));
}
void oled_segment(int x0,int y0,int x1,int y1)
{
    int dx=x1>x0?x1-x0:x0-x1;
    int dy=y1>y0?y1-y0:y0-y1;
    int sx=x0<x1?1:-1;
    int sy=y0<y1?1:-1;
    int erreur=dx-dy;
    while(true){
        oled_pixel(x0,y0);
        if(x0==x1 && y0==y1) break;
        int e2=2*erreur;
        if(e2> -dy) {erreur-=dy;x0+=sx;}
        if(e2<  dx) {erreur+=dx;y0+=sy;}
    }
}
void oled_actualiser(void)
{
    if(!present)return;
    for(int p=0;p<OLED_PAGES;p++) {
        /* Offset visible du SH1106: +2 colonnes en RAM 132 pixels. */
        uint8_t cmd[]={ (uint8_t)(0xB0|p),0x02,0x10 };
        if(commande(cmd,sizeof(cmd))!=ESP_OK) return;
        uint8_t data[1+OLED_LARGEUR];
        data[0]=0x40;
        memcpy(data+1,image[p],OLED_LARGEUR);
        if(i2c_master_transmit(device,data,sizeof(data),1000)!=ESP_OK) return;
    }
}

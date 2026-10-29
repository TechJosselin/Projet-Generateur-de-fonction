/* Verifie les pixels produits par le vrai oled.c, pas une police de substitution. */
#include "oled.h"
#include "driver/i2c_master.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks,transferts,page_courante;
static uint8_t dalle[8][128];
struct fake_device {int unused;};
static struct fake_device dev;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"ECHEC OLED %d: %s\n",__LINE__,#x);exit(1);}}while(0)
esp_err_t i2c_commun_initialiser(void){return ESP_OK;}
i2c_master_bus_handle_t i2c_commun_bus(void){return &dev;}
esp_err_t i2c_master_probe(i2c_master_bus_handle_t b,uint16_t a,int ms){(void)b;(void)a;(void)ms;return ESP_OK;}
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t b,const i2c_device_config_t *c,i2c_master_dev_handle_t *d){(void)b;(void)c;*d=&dev;return ESP_OK;}
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t d,const uint8_t *b,size_t n,int ms){
    CHECK(d==&dev && ms>0 && ms<=50);
    if(n==4 && b[0]==0){CHECK(b[1]>=0xB0 && b[1]<=0xB7 && b[2]==2 && b[3]==0x10);page_courante=b[1]&7;}
    else if(n==129){CHECK(b[0]==0x40);memcpy(dalle[page_courante],b+1,128);transferts++;}
    return ESP_OK;
}
static void vider(void){
    for(unsigned t=0;t<8;t++){unsigned avant=transferts;oled_periodique(t);CHECK(transferts-avant<=1);}
}
static bool pixel(int x,int y){return (dalle[y/8][x]&(1U<<(y%8)))!=0;}
static void verifier_glyphe(void){
    static const uint8_t a[]={0x7e,0x11,0x11,0x11,0x7e};
    oled_effacer();oled_texte_grand(4,14,"A");vider();
    for(int y=0;y<64;y++)for(int x=0;x<128;x++){
        bool attendu=false;
        for(int c=0;c<5;c++)if(x>=4+(c*3)/2 && x<4+((c+1)*3)/2 && y>=14 && y<28)
            attendu=(a[c]&(1U<<((y-14)/2)))!=0;
        CHECK(pixel(x,y)==attendu);
    }
    unsigned avant=transferts;oled_texte_grand(4,14,"A");vider();CHECK(transferts==avant);
    uint8_t copie[8][128];memcpy(copie,dalle,sizeof(copie));
    oled_inverser_zone(0,13,128,16);vider();
    for(int y=0;y<64;y++)for(int x=0;x<128;x++){
        bool ancien=(copie[y/8][x]&(1U<<(y%8)))!=0;
        CHECK(pixel(x,y)==(ancien^(y>=13 && y<29)));
    }
    oled_inverser_zone(0,13,128,16);vider();CHECK(!memcmp(copie,dalle,sizeof(copie)));
}
static void verifier_bornes(void){
    oled_effacer();vider();unsigned avant=transferts;
    oled_texte_grand(INT_MIN,0,"A");oled_texte_grand(INT_MAX,0,"A");
    oled_texte_grand(0,INT_MIN,"A");oled_texte_grand(0,INT_MAX,"A");
    oled_texte_grand(0,0,NULL);
    oled_inverser_zone(INT_MAX,0,INT_MAX,64);oled_inverser_zone(INT_MIN,0,INT_MAX,64);
    oled_inverser_zone(0,INT_MAX,128,INT_MAX);oled_inverser_zone(0,INT_MIN,128,INT_MAX);
    oled_inverser_zone(0,0,-1,64);vider();CHECK(transferts==avant);
    oled_inverser_zone(-1,-1,3,3);vider();
    for(int y=0;y<64;y++)for(int x=0;x<128;x++)CHECK(pixel(x,y)==(x<2 && y<2));
    oled_effacer();oled_inverser_zone(126,62,INT_MAX,INT_MAX);vider();
    for(int y=0;y<64;y++)for(int x=0;x<128;x++)CHECK(pixel(x,y)==(x>=126 && y>=62));
    oled_effacer();oled_texte_grand(-2,-2,"A");oled_texte_grand(120,60,"A");vider();
    CHECK(pixel(120,62));
}
static void verifier_mise_en_page(void){
    oled_effacer();oled_ligne(0,"GENERATEUR / EQ1",true);oled_segment(0,10,127,10);
    oled_texte_grand(4,14,"SIGNAUX");oled_texte_grand(4,31,"SORTIE / ARR>");
    oled_texte_grand(4,48,"MEMOIRE / PR>");oled_inverser_zone(0,13,128,16);vider();
    for(int x=0;x<128;x++){CHECK(pixel(x,10));CHECK(!pixel(x,63));}
    /* Image de controle issue du framebuffer transmis, jamais d'une carte. */
    FILE *f=fopen("build/native/oled-3-lignes.pbm","wb");
    if(f){fprintf(f,"P1\n128 64\n");for(int y=0;y<64;y++)for(int x=0;x<128;x++)fprintf(f,"%d%c",pixel(x,y)?0:1,x==127?'\n':' ');fclose(f);}
}
int main(void){CHECK(oled_initialiser());verifier_glyphe();verifier_bornes();verifier_mise_en_page();
    printf("RESULTAT rendu OLED : %u assertions. Pixels simules, pas d'ecran physique.\n",checks);return 0;}

#include "menu.h"
#include "integration.h"
#include "usb_service.h"
#include "oled.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks,arrets;
static char lignes[8][22];
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"ECHEC UI %d: %s\n",__LINE__,#x);exit(1);}}while(0)
void oled_effacer(void){memset(lignes,0,sizeof(lignes));}
void oled_ligne(int l,const char *p,bool inv){(void)inv;if(strlen(p)>21)fprintf(stderr,"LIGNE TROP LONGUE: %s\n",p);CHECK(l>=0 && l<8 && strlen(p)<=21);strcpy(lignes[l],p);}
void oled_pixel(int x,int y){CHECK(x>=0 && x<128 && y>=0 && y<64);}
void oled_segment(int x,int y,int x1,int y1){oled_pixel(x,y);oled_pixel(x1,y1);}
uint32_t oled_erreurs(void){return 0;}
uint32_t encodeur_pertes(void){return 0;}
bool usb_transport_disponible(void){return true;}
static gf_result_t stop(void *ctx){(void)ctx;arrets++;return GF_OK;}
static void initialiser(void) {
    integration_backend_t b={.dds=true,.arreter=stop};stockage_io_t io={0};
    integration_initialiser_avec(b,io);menu_initialiser();usb_service_initialiser();
}
static void clic(void){menu_traiter(UI_CLIC);}
static void plus(unsigned n){while(n--)menu_traiter(UI_ROTATION_PLUS);}
static void dessin(uint32_t t){menu_periodique(t);}
static void entree(void) {
    quadrature_t q;quadrature_initialiser(&q,3);
    const uint8_t f[]={2,0,1,3},r[]={1,0,2,3};int somme=0;
    for(unsigned i=0;i<4;i++){somme+=quadrature_traiter(&q,f[i]);}CHECK(somme==1);
    somme=0;for(unsigned i=0;i<4;i++)somme+=quadrature_traiter(&q,r[i]);CHECK(somme==-1);
    const uint8_t bruit[]={2,3,2,3,1,3,0,3};somme=0;
    for(unsigned i=0;i<sizeof(bruit);i++){somme+=quadrature_traiter(&q,bruit[i]);}CHECK(somme==0);
    bouton_t b;bouton_initialiser(&b,false,0);
    CHECK(bouton_traiter(&b,true,10)==UI_AUCUN);CHECK(bouton_traiter(&b,false,15)==UI_AUCUN);
    CHECK(bouton_traiter(&b,true,20)==UI_AUCUN);CHECK(bouton_traiter(&b,true,50)==UI_AUCUN);
    CHECK(bouton_traiter(&b,false,100)==UI_AUCUN);CHECK(bouton_traiter(&b,false,130)==UI_CLIC);
    CHECK(bouton_traiter(&b,false,200)==UI_AUCUN);
    CHECK(bouton_traiter(&b,true,300)==UI_AUCUN);CHECK(bouton_traiter(&b,true,330)==UI_AUCUN);
    CHECK(bouton_traiter(&b,true,1130)==UI_APPUI_LONG);CHECK(bouton_traiter(&b,true,2000)==UI_AUCUN);
    CHECK(bouton_traiter(&b,false,2010)==UI_AUCUN);CHECK(bouton_traiter(&b,false,2040)==UI_AUCUN);
    bouton_initialiser(&b,false,UINT32_MAX-100);(void)bouton_traiter(&b,true,UINT32_MAX-50);
    CHECK(bouton_traiter(&b,true,10)==UI_AUCUN);CHECK(bouton_traiter(&b,true,810)==UI_APPUI_LONG);
    puts("PASS entree : rebonds, quadrature, transitions invalides, clic/long et rebouclage horloge.");
}
static void parcours(void) {
    initialiser();dessin(0);CHECK(strstr(lignes[0],"GENERATEUR"));
    clic();dessin(1);CHECK(strstr(lignes[0],"SIGNAUX"));
    clic();clic();dessin(2);CHECK(integration_statut()->selection==0);
    clic();plus(2);clic();dessin(3);CHECK(strstr(lignes[0],"BROUILLON"));
    plus(2);uint32_t revision=integration_statut()->revision;
    signal_config_t s=*signaux_lire(0);s.amplitude_mvpp=1500;
    CHECK(integration_modifier(0,s,0)==GF_OK);clic();dessin(4);
    CHECK(!strcmp(lignes[2],"CONFLIT"));CHECK(signaux_lire(0)->amplitude_mvpp==1500);
    CHECK(integration_statut()->revision==revision+1);
    unsigned av=arrets;menu_traiter(UI_APPUI_LONG);CHECK(arrets>av);
    /* Chaque entree de menu et confirmation ; aucune ligne tronquee. */
    for(unsigned h=0;h<6;h++) {
        initialiser();plus(h);clic();dessin(10+h);
        for(unsigned n=0;n<12;n++){plus(1);dessin(30+n);clic();dessin(60+n);menu_traiter(UI_APPUI_LONG);dessin(90+n);}
    }
    /* Catalogue redimensionne depuis USB alors que la selection UI est a la fin. */
    initialiser();for(unsigned i=10;i<100;i++)CHECK(integration_dupliquer(0,NULL)==GF_OK);
    clic();plus(99);dessin(200);CHECK(strstr(lignes[0],"100/100"));
    CHECK(integration_usine()==GF_OK);dessin(500);CHECK(strstr(lignes[0],"10/10"));
    /* Parcours pseudo-aleatoires reproductibles, avec mutations hors UI. */
    uint32_t x=7;
    for(unsigned i=0;i<10000;i++) {
        x=x*1664525U+1013904223U;menu_traiter((evenement_ui_t)(1+x%4));dessin(1000+i*20);
        if(i%99==0)(void)integration_usine();
    }
    puts("PASS UI : conflit brouillon/USB, arret long, navigation et catalogue 100 vers 10.");
}
int main(void){entree();parcours();printf("RESULTAT UI : %u assertions. Aucun ecran physique.\n",checks);return 0;}

#include "menu.h"
#include "integration.h"
#include "usb_service.h"
#include "oled.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks,arrets,commandes,activations,epots;
static bool panne_arret,panne_configuration;
static char lignes[8][22],grandes[3][OLED_GRAND_NB_CARACTERES+1];
static bool inverse[8],separateur;
static unsigned nb_grandes,nb_zones;
static int selection_visible;
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"ECHEC UI %d: %s\n",__LINE__,#x);exit(1);}}while(0)
void oled_effacer(void) {
    memset(lignes,0,sizeof(lignes));memset(grandes,0,sizeof(grandes));memset(inverse,0,sizeof(inverse));
    nb_grandes=0;nb_zones=0;selection_visible=-1;separateur=false;
}
void oled_ligne(int l,const char *p,bool inv) {
    CHECK(l>=0 && l<8 && strlen(p)<=21);strcpy(lignes[l],p);inverse[l]=inv;
}
void oled_pixel(int x,int y){CHECK(x>=0 && x<128 && y>=0 && y<64);}
void oled_segment(int x,int y,int x1,int y1) {
    oled_pixel(x,y);oled_pixel(x1,y1);
    if(x==0 && y==10 && x1==127 && y1==10)separateur=true;
}
void oled_texte_grand(int x,int y,const char *p) {
    CHECK(nb_grandes<3 && x==4 && y==14+(int)nb_grandes*17);
    CHECK(strlen(p)<=OLED_GRAND_NB_CARACTERES);strcpy(grandes[nb_grandes++],p);
}
void oled_inverser_zone(int x,int y,int largeur,int hauteur) {
    CHECK(x==0 && largeur==128 && hauteur==16 && (y==13 || y==30 || y==47));
    CHECK(nb_zones==0);nb_zones++;selection_visible=(y-13)/17;
}
static gf_result_t stop(void *ctx){(void)ctx;arrets++;return panne_arret?GF_IO:GF_OK;}
static gf_result_t configurer(void *ctx,const signal_config_t *s) {
    (void)ctx;CHECK(s!=NULL);commandes++;return panne_configuration?GF_IO:GF_OK;
}
static gf_result_t epot(void *ctx,uint8_t code){(void)ctx;(void)code;epots++;return GF_OK;}
static gf_result_t activer(void *ctx){(void)ctx;activations++;return GF_OK;}
/* Profil SYNTHETIQUE, uniquement dans le test. Jamais importe au boot du produit. */
static calibration_t fixture(void) {
    calibration_t p={0};p.approuve=true;p.gain_avant_offset=true;
    strcpy(p.carte,"TEST-SYNTHETIQUE");p.charge_ohm=1000000;p.sortie_min_mv=-12000;p.sortie_max_mv=12000;
    for(unsigned f=0;f<3;f++) {
        cal_forme_t *v=&p.formes[f];v->freq_min_hz=1;v->freq_max_hz=12000000;
        v->erreur_amplitude_mvpp=100;v->erreur_offset_mv=100;v->nombre_gain=v->nombre_offset=2;
        v->gain[0]=(cal_gain_t){0,100,(int32_t)(f*100)};v->gain[1]=(cal_gain_t){255,20000,(int32_t)(f*1000)};
        v->offset[0]=(cal_offset_t){0,-15000};v->offset[1]=(cal_offset_t){255,15000};
    }return p;
}
static void initialiser(bool calibre) {
    panne_arret=panne_configuration=false;
    integration_backend_t b={.i2c=true,.dds=true,.epots=true,.arreter=stop,
        .dds_configurer=configurer,.gain=epot,.offset=epot,.activer=activer};
    stockage_io_t io={0};integration_initialiser_avec(b,io);
    if(calibre) {
        calibration_t p=fixture();uint8_t image[CAL_IMAGE_SIZE];
        CHECK(calibration_encoder(&p,image)==GF_OK);CHECK(integration_importer_calibration(image)==GF_OK);
    }
    menu_initialiser();usb_service_initialiser();arrets=commandes=activations=epots=0;
}
static void clic(void){menu_traiter(UI_CLIC);}
static void plus(unsigned n){while(n--)menu_traiter(UI_ROTATION_PLUS);}
static void moins(unsigned n){while(n--)menu_traiter(UI_ROTATION_MOINS);}
static void dessin(uint32_t t) {
    menu_periodique(t);
    if(nb_grandes) {
        CHECK(nb_grandes<=3 && nb_zones<=1 && separateur && inverse[0]);
        if(nb_zones)CHECK(selection_visible>=0 && (unsigned)selection_visible<nb_grandes);
        for(unsigned i=1;i<8;i++)CHECK(lignes[i][0]==0);
    }
}
static void usb(const char *trame,uint32_t t,char *reponse,size_t capacite) {
    for(size_t i=0;trame[i];i++)usb_service_octet((uint8_t)trame[i],t);
    size_t total=0,n;const uint8_t *p;
    while((p=usb_service_tx(&n))!=NULL) {
        CHECK(total+n<capacite);memcpy(reponse+total,p,n);total+=n;usb_service_tx_consomme(n,t);
    }
    reponse[total]=0;
}
static void entree(void) {
    quadrature_t q;quadrature_initialiser(&q,3);
    const uint8_t f[]={2,0,1,3},r[]={1,0,2,3};int somme=0;
    for(unsigned i=0;i<4;i++)somme+=quadrature_traiter(&q,f[i]);
    CHECK(somme==1);somme=0;
    for(unsigned i=0;i<4;i++)somme+=quadrature_traiter(&q,r[i]);
    CHECK(somme==-1);
    const uint8_t bruit[]={2,3,2,3,1,3,0,3};somme=0;
    for(unsigned i=0;i<sizeof(bruit);i++)somme+=quadrature_traiter(&q,bruit[i]);
    CHECK(somme==0);
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
    puts("PASS entree : rebonds, quadrature, transitions invalides, clic/long et horloge.");
}
static void selection_directe(void) {
    initialiser(true);dessin(0);CHECK(strstr(lignes[0],"SIG 1/10 OFF"));
    CHECK(commandes==0 && activations==0 && integration_statut()->selection==-1);
    uint32_t revision=integration_statut()->revision;
    plus(2);dessin(1);CHECK(commandes==0 && activations==0);
    clic();dessin(2);
    CHECK(integration_statut()->selection==2 && integration_statut()->etat==SYS_ACTIF_COMMANDE);
    CHECK(commandes==1 && activations==1 && integration_statut()->commande_courante);
    CHECK(signaux_egaux(integration_statut()->derniere,*signaux_lire(2)));
    CHECK(nb_grandes==3 && nb_zones==0 && strstr(lignes[0],"S3 SIN / ON"));
    CHECK(!strcmp(grandes[0],"F 1kHz") && !strcmp(grandes[1],"A 2.000Vpp") && !strcmp(grandes[2],"O +3.000V"));
    clic();dessin(3);CHECK(nb_zones==1 && activations==1 && strstr(lignes[0],"ON S3"));
    plus(1);dessin(4);CHECK(integration_statut()->selection==2 && activations==1);
    clic();dessin(5);CHECK(integration_statut()->selection==3 && activations==2);
    moins(1);dessin(6);CHECK(nb_zones==1 && integration_statut()->selection==3 && activations==2);
    clic();CHECK(integration_statut()->selection==2 && activations==3);
    unsigned av=arrets;menu_traiter(UI_APPUI_LONG);dessin(7);
    CHECK(arrets>av && integration_statut()->etat!=SYS_ACTIF_COMMANDE && strstr(lignes[0],"OFF"));
    CHECK(integration_statut()->revision==revision);
    menu_traiter(UI_AUCUN);CHECK(activations==3);
    puts("PASS selection : un clic active, retour et rotation sans commande, appui long arrete.");
}
static void sans_calibration(void) {
    for(unsigned i=0;i<SIGNAL_INIT_COUNT;i++) {
        initialiser(false);uint32_t revision=integration_statut()->revision;
        plus(i);clic();dessin(0);
        CHECK(activations==1 && commandes==1 && epots==0);
        CHECK(strstr(lignes[0],"DDS SEUL") && !strcmp(grandes[1],"A NON REGLEE") && !strcmp(grandes[2],"O NON REGLE"));
        CHECK(!integration_statut()->commande_courante && !integration_calibration()->approuve);
        CHECK(integration_statut()->revision==revision);
        clic();dessin(1);CHECK(strstr(lignes[0],"DDS S") && activations==1);
        menu_traiter(UI_APPUI_LONG);dessin(2);CHECK(strstr(lignes[0],"OFF"));
    }
    initialiser(false);clic();char reponse[512];
    usb("$STATE?#",0,reponse,sizeof(reponse));CHECK(strstr(reponse,"DDS_SEUL_COMMANDE"));
    usb("$1:SIN:1500:+250:12345#",1,reponse,sizeof(reponse));
    CHECK(strstr(reponse,"$OK:RAM:") && integration_statut()->etat!=SYS_ACTIF_COMMANDE);
    clic();clic();dessin(2);CHECK(!strcmp(grandes[0],"F 12345Hz") && epots==0);
    usb("$1?#",3,reponse,sizeof(reponse));CHECK(!strcmp(reponse,"$1:SIN:1500:+250:12345#"));
    /* Importer un profil ne reactive pas tout seul la sortie. */
    calibration_t p=fixture();uint8_t image[CAL_IMAGE_SIZE];
    CHECK(calibration_encoder(&p,image)==GF_OK);CHECK(integration_importer_calibration(image)==GF_OK);
    dessin(300);CHECK(strstr(lignes[0],"OFF"));
    clic();clic();dessin(301);CHECK(strstr(lignes[0],"/ ON") && epots==2);
    CHECK(!strcmp(grandes[1],"A 1.500Vpp") && !strcmp(grandes[2],"O +0.250V"));
    puts("PASS UI DDS seul : clic sans profil, indications A/O non regles, USB et passage au mode calibre.");
}
static void lecture_seule_et_usb(void) {
    initialiser(true);signal_config_t copie[SIGNAL_INIT_COUNT];
    for(size_t i=0;i<SIGNAL_INIT_COUNT;i++)copie[i]=*signaux_lire(i);
    uint8_t profil[CAL_IMAGE_SIZE],apres[CAL_IMAGE_SIZE];
    CHECK(calibration_encoder(integration_calibration(),profil)==GF_OK);
    uint32_t revision=integration_statut()->revision,x=7;
    for(unsigned i=0;i<10000;i++) {
        x=x*1664525U+1013904223U;menu_traiter((evenement_ui_t)(1+(x>>16)%4));dessin(i*20);
        CHECK(integration_statut()->revision==revision && signaux_nombre()==SIGNAL_INIT_COUNT);
    }
    for(size_t i=0;i<SIGNAL_INIT_COUNT;i++)CHECK(signaux_egaux(copie[i],*signaux_lire(i)));
    CHECK(calibration_encoder(integration_calibration(),apres)==GF_OK);CHECK(!memcmp(profil,apres,sizeof(profil)));
    initialiser(true);clic();dessin(0);revision=integration_statut()->revision;
    char reponse[8192];usb("$1:SIN:1500:+250:12345#",1,reponse,sizeof(reponse));
    CHECK(strstr(reponse,"$OK:RAM:") && integration_statut()->revision==revision+1);
    CHECK(signaux_lire(0)->amplitude_mvpp==1500 && signaux_lire(0)->offset_mv==250 && signaux_lire(0)->frequence_hz==12345);
    CHECK(!integration_statut()->commande_courante && activations==1);
    dessin(300);CHECK(strstr(lignes[0],"OFF") && !strcmp(grandes[0],"F 12345Hz"));
    usb("$1?#",301,reponse,sizeof(reponse));CHECK(!strcmp(reponse,"$1:SIN:1500:+250:12345#"));
    usb("$?#",302,reponse,sizeof(reponse));unsigned trames=0;
    for(size_t i=0;reponse[i];i++)if(reponse[i]=='$')trames++;
    CHECK(trames==SIGNAL_INIT_COUNT);
    usb("$1:SIN:INVALIDE:+0:1000#",303,reponse,sizeof(reponse));
    CHECK(strstr(reponse,"$ERR:") && integration_statut()->revision==revision+1);
    clic();clic();dessin(304);CHECK(activations==2 && integration_statut()->derniere.frequence_hz==12345);
    puts("PASS lecture seule : 10000 evenements sans mutation, ecriture/lectures USB du cahier des charges.");
}
static void erreurs(void) {
    initialiser(false);panne_configuration=true;clic();dessin(0);
    CHECK(!strcmp(lignes[0],"SORTIE NON ACTIVEE") && !strcmp(lignes[2],"IO"));
    CHECK(activations==0 && commandes==1 && epots==0);clic();dessin(1);CHECK(nb_zones==1);
    initialiser(true);panne_configuration=true;clic();dessin(0);
    CHECK(integration_statut()->etat==SYS_DEFAUT && activations==0 && commandes==1);
    CHECK(!strcmp(lignes[0],"SORTIE NON ACTIVEE"));
    panne_arret=true;menu_traiter(UI_APPUI_LONG);dessin(1);
    CHECK(!strcmp(lignes[0],"ARRET NON CONFIRME") && integration_statut()->etat==SYS_DEFAUT);
    panne_arret=panne_configuration=false;menu_traiter(UI_APPUI_LONG);clic();
    CHECK(integration_statut()->etat==SYS_ACTIF_COMMANDE && activations==1);
    /* Selection refusee : surtout ne pas reactiver l'ancien signal. */
    plus(1);panne_arret=true;clic();dessin(2);
    CHECK(integration_statut()->etat==SYS_DEFAUT && integration_statut()->selection==0 && activations==1);
    CHECK(!strcmp(lignes[0],"SORTIE NON ACTIVEE"));
    puts("PASS erreurs : defauts conserves avec/sans profil, pas d'activation apres selection refusee.");
}
static void listes_agrandies(void) {
    initialiser(true);dessin(0);CHECK(nb_grandes==3 && selection_visible==0);
    CHECK(!strncmp(grandes[0],"01 ",3) && grandes[1][OLED_GRAND_NB_CARACTERES-1]=='>');
    plus(2);dessin(1);CHECK(selection_visible==2);
    char avant[OLED_GRAND_NB_CARACTERES+1],fixe[OLED_GRAND_NB_CARACTERES+1];
    strcpy(avant,grandes[2]);strcpy(fixe,grandes[0]);
    dessin(900);CHECK(!strcmp(avant,grandes[2]));dessin(1251);
    CHECK(strcmp(avant,grandes[2]) && !strcmp(fixe,grandes[0]));
    plus(1);dessin(1252);CHECK(selection_visible==2 && !strncmp(grandes[0],"02 ",3));
    moins(3);dessin(1253);CHECK(selection_visible==0 && !strncmp(grandes[0],"01 ",3));
    plus(99);dessin(1254);CHECK(selection_visible==2 && !strncmp(grandes[0],"08 ",3));
    initialiser(true);dessin(2000);CHECK(selection_visible==0 && !strncmp(grandes[0],"01 ",3));
    plus(2);dessin(UINT32_MAX-500U);strcpy(avant,grandes[2]);dessin(999);CHECK(strcmp(avant,grandes[2]));
    moins(1);plus(1);dessin(1000);CHECK(!strcmp(avant,grandes[2]));
    initialiser(true);for(unsigned i=10;i<100;i++)CHECK(integration_dupliquer(0,NULL)==GF_OK);
    plus(99);dessin(0);CHECK(!strncmp(grandes[0],"98 ",3) && !strncmp(grandes[2],"100 ",4));
    CHECK(integration_usine()==GF_OK);
    clic();dessin(1);CHECK(integration_statut()->selection==9 && activations==1); /* Sans dessin intermediaire. */
    clic();dessin(2);CHECK(!strncmp(grandes[0],"08 ",3) && !strncmp(grandes[2],"10 ",3));
    while(signaux_nombre()>1)CHECK(integration_supprimer(signaux_nombre()-1)==GF_OK);
    dessin(300);CHECK(nb_grandes==1 && selection_visible==0 && !strncmp(grandes[0],"01 ",3));
    unsigned av=arrets;dessin(2000);CHECK(arrets==av);
    /* Valeurs longues : aucune troncature dans la fiche en grande police. */
    initialiser(true);char reponse[128];usb("$1:SIN:20000:+0:11999999#",0,reponse,sizeof(reponse));
    clic();dessin(0);CHECK(!strcmp(grandes[0],"F 11999999Hz") && !strcmp(grandes[1],"A 20.000Vpp"));
    usb("$1:SIN:200:-10000:1#",1,reponse,sizeof(reponse));clic();clic();dessin(1);
    CHECK(!strcmp(grandes[2],"O -10.000V"));
    puts("PASS affichage : trois lignes, defilement, bornes, horloge, catalogue redimensionne et valeurs exactes.");
}
int main(void) {
    entree();selection_directe();sans_calibration();lecture_seule_et_usb();erreurs();listes_agrandies();
    printf("RESULTAT UI : %u assertions. Aucun ecran physique.\n",checks);return 0;
}

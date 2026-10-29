/* ===========================================================================
 * Interface OLED SH1106 - tous les menus du generateur de fonctions.
 * Edition des signaux en RAM et diagnostic temps reel du materiel raccorde.
 * Navigation: rotation, clic court, clic long (0.8 s).
 * =========================================================================== */
#include "menu.h"
#include "signaux.h"
#include "integration.h"
#include "integration_compat.h"
#include "oled.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "driver/gpio.h"
#include "esp_timer.h"

#define NB_ACCUEIL 7
#define NB_DETAIL 9
#define NB_SORTIE 6
#define NB_MEMOIRE 5
#define NB_USB 4
#define NB_REGLAGES 4
#define NB_DIAG 4
#define PAS_FREQ_N 8
#define PAS_VOLT_N 4

static const uint32_t pas_freq[PAS_FREQ_N]={1,10,100,1000,10000,100000,1000000,10000000};
static const uint32_t pas_volt[PAS_VOLT_N]={1,10,100,1000};
static const char *accueil[NB_ACCUEIL]={
    "Signaux predefinis", "Sortie analogique", "Memoire EEPROM",
    "Communication USB", "Diagnostic", "Reglages", "Informations"
};
static const char *detail_fin[4]={"Apercu graphique","Dupliquer signal","Supprimer signal","Retour liste"};
static const char *sortie_options[NB_SORTIE]={
    "Etat / Signal", "Activer DDS test", "Couper DDS test", "Sorties digitales", "Materiel analog.", "Retour"
};
static const char *memoire_options[NB_MEMOIRE]={
    "Etat de la RAM", "Sauver en EEPROM", "Charger EEPROM", "Presets d'usine", "Retour"
};
static const char *usb_options[NB_USB]={
    "Etat communication", "Format ecriture", "Format lecture", "Retour"
};
static const char *reglages_options[NB_REGLAGES]={
    "Pas frequence", "Pas amplitude", "Pas offset", "Retour"
};

typedef enum {
    PAGE_ACCUEIL, PAGE_LISTE, PAGE_DETAIL, PAGE_EDITION, PAGE_APERCU,
    PAGE_SORTIE, PAGE_SORTIE_INFO, PAGE_DIGITALES, PAGE_MATERIEL,
    PAGE_MEMOIRE, PAGE_RAM_INFO, PAGE_USB, PAGE_USB_INFO,
    PAGE_USB_ECRITURE, PAGE_USB_LECTURE, PAGE_DIAGNOSTIC,
    PAGE_REGLAGES, PAGE_EDIT_PAS, PAGE_INFOS,
    PAGE_CONFIRM, PAGE_MESSAGE
} page_t;
typedef enum {
    CONFIRMER_RIEN, CONFIRMER_SUPPRESSION, CONFIRMER_USINE,
    CONFIRMER_SAUVER, CONFIRMER_CHARGER, CONFIRMER_VOUT_ON
} confirmer_t;

static page_t page, apres_message, apres_pas;
static confirmer_t confirmation;
static bool choix_oui;
static bool redessiner;
static int64_t message_jusque,dernier_diag;
static int c_accueil,c_signal,c_detail,c_sortie,c_memoire,c_usb,c_reglages,c_diag;
static int pas_freq_idx=3,pas_ampl_idx=2,pas_offset_idx=2;
static int pas_en_cours;
static int champ_pas;   /* 0 freq; 1 amplitude; 2 offset */
static signal_champ_t champ_edit;
static signal_config_t brouillon;
static uint32_t revision_brouillon;
static char titre_message[28],ligne_message1[28],ligne_message2[28];
static uint32_t compteur_plus,compteur_moins,compteur_clic,compteur_long;
/* Fenetre de 3 lignes, independante pour chaque page de liste. */
static page_t page_fenetre=(page_t)-1;
static int premiere_ligne_visible;
static int64_t dernier_evenement_us,derniere_animation_us;
static bool defilement_necessaire;

static void aller(page_t nouvelle){
    if(page!=nouvelle){
        dernier_evenement_us=esp_timer_get_time();
        defilement_necessaire=false;
    }
    page=nouvelle;
    redessiner=true;
}
static int borne(int v,int mini,int maxi){return v<mini?mini:(v>maxi?maxi:v);}
static void titre(const char *s){oled_ligne(0,s,true);}
static void pied(const char *s){oled_ligne(7,s,false);}
static void message(const char *titre_s,const char *l1,const char *l2,page_t ensuite){
    snprintf(titre_message,sizeof(titre_message),"%s",titre_s);
    snprintf(ligne_message1,sizeof(ligne_message1),"%s",l1);
    snprintf(ligne_message2,sizeof(ligne_message2),"%s",l2);
    apres_message=ensuite;
    message_jusque=esp_timer_get_time()+1900000LL;
    aller(PAGE_MESSAGE);
}
static const signal_config_t *actuel(void){return signaux_lire((size_t)c_signal);}

static void retour(void){
    switch(page){
    case PAGE_LISTE: case PAGE_SORTIE: case PAGE_MEMOIRE: case PAGE_USB:
    case PAGE_DIAGNOSTIC: case PAGE_REGLAGES: case PAGE_INFOS:
        aller(PAGE_ACCUEIL);break;
    case PAGE_DETAIL:aller(PAGE_LISTE);break;
    case PAGE_EDIT_PAS:aller(PAGE_REGLAGES);break;
    case PAGE_EDITION: case PAGE_APERCU:aller(PAGE_DETAIL);break;
    case PAGE_SORTIE_INFO:case PAGE_DIGITALES:case PAGE_MATERIEL:
        aller(PAGE_SORTIE);break;
    case PAGE_RAM_INFO:aller(PAGE_MEMOIRE);break;
    case PAGE_USB_INFO:case PAGE_USB_ECRITURE:case PAGE_USB_LECTURE:
        aller(PAGE_USB);break;
    case PAGE_CONFIRM:

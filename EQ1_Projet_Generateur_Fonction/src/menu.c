/* Navigation et commandes de finalisation ; rendu des listes adapte depuis
 * feature/menu-oled-3-lignes (f781dbf). Aucun acces pilote depuis le menu. */
#include "menu.h"
#include "integration.h"
#include "oled.h"
#include "encodeur.h"
#include "usb_service.h"
#include "usb_transport.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <inttypes.h>
typedef enum {ACCUEIL,LISTE,DETAIL,EDITION,APERCU,SORTIE,MEMOIRE,DIAG,DIGITAL,PAS,CONFIRMATION,MESSAGE,SORTIE_INFO,MEMOIRE_INFO,DIGITAL_INFO} page_t;
static page_t page,retour_message,retour_confirmation;
static int curseur,index_signal,champ,confirmation,canal;
static bool oui,sale;
static signal_config_t brouillon;
static uint32_t revision_brouillon,now,dernier_dessin,expiration;
static gf_result_t dernier_resultat;
/* Fenetre conservee tant que la page ne change pas. Horloge en ms modulo 2^32. */
static page_t page_fenetre;
static int premiere_ligne;
static bool fenetre_valide,action_en_attente,defilement_necessaire;
static uint32_t dernier_evenement,derniere_animation;
static uint32_t freq_dig[2]={1000,1000};
static uint16_t duty_dig[2]={500,500};
static const uint32_t pas[]={1,10,100,1000,10000,100000};
static unsigned pas_freq=3,pas_tension=2;
static const char *const accueil[]={"SIGNAUX","SORTIE / ARRET","MEMOIRE / PROFIL","DIAGNOSTIC / USB","NUMERIQUES","PAS DE REGLAGE"};
static int borne(int x,int lo,int hi){return x<lo?lo:(x>hi?hi:x);}
static void aller(page_t p){
    if(page!=p)fenetre_valide=false;
    page=p;curseur=0;sale=true;action_en_attente=true;defilement_necessaire=false;
}
static void texte(int ligne,const char *format,...) {
    char b[64];va_list a;va_start(a,format);vsnprintf(b,sizeof(b),format,a);va_end(a);oled_ligne(ligne,b,false);
}
/* Bandeau fixe a y=0 ; trois lignes a y=14,31,48, comme la branche source.
 * Aucun pied de page : il chevaucherait la troisieme ligne. Les etats detailles
 * restent accessibles dans les pages INFO et dans DIAGNOSTIC. */
static void liste(const char *titre,int selection,int total,void (*libelle)(int,char *,size_t)) {
    oled_ligne(0,titre,true);oled_segment(0,10,127,10);
    defilement_necessaire=false;
    if(total<=0)return;
    selection=borne(selection,0,total-1);
    int max_premiere=total>3?total-3:0;
    if(!fenetre_valide || page_fenetre!=page) {
        premiere_ligne=borne(selection-1,0,max_premiere);
        page_fenetre=page;fenetre_valide=true;
    }
    if(selection<premiere_ligne)premiere_ligne=selection;
    if(selection>=premiere_ligne+3)premiere_ligne=selection-2;
    premiere_ligne=borne(premiere_ligne,0,max_premiere);
    for(int i=0;i<3 && premiere_ligne+i<total;i++) {
        char complet[64]={0},visible[OLED_GRAND_NB_CARACTERES+1];
        libelle(premiere_ligne+i,complet,sizeof(complet));
        bool choisi=premiere_ligne+i==selection;
        size_t longueur=strlen(complet),decalage=0;
        if(choisi && longueur>OLED_GRAND_NB_CARACTERES) {
            defilement_necessaire=true;
            uint32_t ecoule=now-dernier_evenement;
            if(ecoule>900U) {
                size_t max_decalage=longueur-OLED_GRAND_NB_CARACTERES;
                size_t phase=((ecoule-900U)/350U)%(max_decalage+4U);
                decalage=phase>max_decalage?max_decalage:phase;
            }
        }
        size_t n=longueur-decalage;
        if(n>OLED_GRAND_NB_CARACTERES)n=OLED_GRAND_NB_CARACTERES;
        memcpy(visible,complet+decalage,n);visible[n]=0;
        if(!choisi && longueur>OLED_GRAND_NB_CARACTERES)
            visible[OLED_GRAND_NB_CARACTERES-1]='>';
        int y=14+i*17;
        oled_texte_grand(4,y,visible);
        if(choisi)oled_inverser_zone(0,y-1,128,16);
    }
}
static void ligne_accueil(int i,char *b,size_t n){snprintf(b,n,"%s",accueil[i]);}
static void ligne_signal(int i,char *b,size_t n) {
    const signal_config_t *s=signaux_lire((size_t)i);
    if(!s){*b=0;return;}
    char a[24],f[24];
    signaux_formater_tension(a,sizeof(a),s->amplitude_mvpp,false);
    signaux_formater_frequence(f,sizeof(f),s->frequence_hz);
    snprintf(b,n,"%02d %s %s %s",i+1,signaux_type_court(s->forme),a,f);
}
static void ligne_detail(int i,char *b,size_t n) {
    const signal_config_t *s=signaux_lire((size_t)index_signal);
    if(!s){*b=0;return;}
    char v[24];
    if(i==0)snprintf(b,n,"SELECTIONNER");
    else if(i==1)snprintf(b,n,"TYPE %s",signaux_type_court(s->forme));
    else if(i==2)snprintf(b,n,"AMPL %"PRId32" MVPP",s->amplitude_mvpp);
    else if(i==3)snprintf(b,n,"OFFS %"PRId32" MV",s->offset_mv);
    else if(i==4){signaux_formater_frequence(v,sizeof(v),s->frequence_hz);snprintf(b,n,"FREQ %s",v);}
    else snprintf(b,n,"%s",i==5?"APERCU THEORIQUE":i==6?"DUPLIQUER":i==7?"SUPPRIMER":"RETOUR");
}
static void ligne_sortie(int i,char *b,size_t n) {
    static const char *const options[]={"ARRETER TOUT","ACTIVER SELECTION","ETAT / DERNIERE CMD","RETOUR"};
    snprintf(b,n,"%s",options[i]);
}
static void ligne_memoire(int i,char *b,size_t n) {
    static const char *const options[]={"SAUVER MAINTENANT","RECHARGER EEPROM","PRESETS USINE","EFFACER PROFIL","ETAT RAM / PROFIL","RETOUR"};
    snprintf(b,n,"%s",options[i]);
}
static void ligne_digital(int i,char *b,size_t n) {
    if(i==0)snprintf(b,n,"CANAL %d : %s",canal+1,integration_sortie_digitale_disponible((unsigned)canal)?"EQUIPE":"NON CONFIG");
    else if(i==1)snprintf(b,n,"FREQ %"PRIu32" HZ",freq_dig[canal]);
    else if(i==2)snprintf(b,n,"RAPPORT %u/1000",duty_dig[canal]);
    else snprintf(b,n,"%s",i==3?"APPLIQUER / ON":i==4?"ARRETER / OFF":i==5?"ETAT DU CANAL":"RETOUR");
}
static void ligne_pas(int i,char *b,size_t n) {
    if(i==0)snprintf(b,n,"FREQ %"PRIu32" HZ",pas[pas_freq]);
    else if(i==1)snprintf(b,n,"TENSION %"PRIu32" MV",pas[pas_tension]);
    else snprintf(b,n,"RETOUR");
}
static void message(gf_result_t r,page_t retour){dernier_resultat=r;retour_message=retour;expiration=now+1800;aller(MESSAGE);}
static void confirmer(int action,page_t retour){confirmation=action;retour_confirmation=retour;oui=false;aller(CONFIRMATION);}
static void retour(void) {
    if(page==EDITION || page==APERCU)aller(DETAIL);
    else if(page==DETAIL)aller(LISTE);
    else if(page==SORTIE_INFO)aller(SORTIE);
    else if(page==MEMOIRE_INFO)aller(MEMOIRE);
    else if(page==DIGITAL_INFO)aller(DIGITAL);
    else if(page==CONFIRMATION)aller(retour_confirmation);
    else if(page==MESSAGE)aller(retour_message);
    else aller(ACCUEIL);
}
static void valider(void) {
    const signal_config_t *s=signaux_lire((size_t)index_signal);gf_result_t r=GF_OK;
    switch(page) {
    case ACCUEIL: {static const page_t pages[]={LISTE,SORTIE,MEMOIRE,DIAG,DIGITAL,PAS};aller(pages[curseur]);break;}
    case LISTE: aller(DETAIL);break;
    case DETAIL:
        if(!s){aller(LISTE);break;}
        if(curseur==0){r=integration_choisir((size_t)index_signal);message(r,DETAIL);}
        else if(curseur>=1 && curseur<=4){champ=curseur-1;brouillon=*s;revision_brouillon=integration_statut()->revision;aller(EDITION);}
        else if(curseur==5)aller(APERCU);
        else if(curseur==6){int idx=index_signal;r=integration_dupliquer((size_t)index_signal,&idx);index_signal=idx;message(r,DETAIL);}
        else if(curseur==7)confirmer(2,DETAIL);
        else aller(LISTE);
        break;
    case EDITION:r=integration_modifier((size_t)index_signal,brouillon,revision_brouillon);message(r,DETAIL);break;
    case APERCU:aller(DETAIL);break;
    case SORTIE:
        if(curseur==0)message(integration_arreter(),SORTIE);
        else if(curseur==1)confirmer(1,SORTIE);
        else if(curseur==2)aller(SORTIE_INFO);
        else aller(ACCUEIL);
        break;
    case MEMOIRE:
        if(curseur==0)message(integration_sauver(),MEMOIRE);
        else if(curseur==1)confirmer(3,MEMOIRE);
        else if(curseur==2)confirmer(4,MEMOIRE);
        else if(curseur==3)confirmer(5,MEMOIRE);
        else if(curseur==4)aller(MEMOIRE_INFO);
        else aller(ACCUEIL);
        break;
    case DIAG:curseur=(curseur+1)%3;break;
    case DIGITAL:
        if(curseur==0)canal=1-canal;
        else if(curseur==1){freq_dig[canal]=freq_dig[canal]>=100000?10:freq_dig[canal]*10;}
        else if(curseur==2)duty_dig[canal]=(uint16_t)((duty_dig[canal]+100)%1100);
        else if(curseur==3)message(integration_commander_digital((unsigned)canal,freq_dig[canal],duty_dig[canal],true),DIGITAL);
        else if(curseur==4)message(integration_commander_digital((unsigned)canal,0,0,false),DIGITAL);
        else if(curseur==5)aller(DIGITAL_INFO);
        else aller(ACCUEIL);
        break;
    case PAS:if(curseur==0)pas_freq=(pas_freq+1)%6;else if(curseur==1)pas_tension=(pas_tension+1)%4;else aller(ACCUEIL);break;
    case CONFIRMATION:
        if(!oui){aller(retour_confirmation);break;}
        if(confirmation==1)r=integration_activer();
        else if(confirmation==2)r=integration_supprimer((size_t)index_signal);
        else if(confirmation==3)r=integration_charger();
        else if(confirmation==4)r=integration_usine();
        else r=integration_effacer_calibration();
        message(r,retour_confirmation);break;
    case MESSAGE:aller(retour_message);break;
    case SORTIE_INFO:case MEMOIRE_INFO:case DIGITAL_INFO:retour();break;
    }
}
void menu_initialiser(void){
    page=ACCUEIL;curseur=0;index_signal=0;canal=0;sale=true;now=0;dernier_dessin=0;
    fenetre_valide=false;premiere_ligne=0;action_en_attente=true;
    defilement_necessaire=false;dernier_evenement=0;derniere_animation=0;
}
void menu_traiter(evenement_ui_t e) {
    if(e!=UI_ROTATION_PLUS && e!=UI_ROTATION_MOINS && e!=UI_CLIC && e!=UI_APPUI_LONG)return;
    action_en_attente=true; /* Date au prochain tick, pas au dernier dessin. */
    if(e==UI_APPUI_LONG){(void)integration_arreter();retour();return;}
    if(e==UI_CLIC){valider();sale=true;return;}
    int d=e==UI_ROTATION_PLUS?1:-1;
    if(page==LISTE)index_signal=borne(index_signal+d,0,(int)signaux_nombre()-1);
    else if(page==EDITION)signaux_variation(&brouillon,(signal_champ_t)champ,d,champ==CHAMP_FREQUENCE?pas[pas_freq]:pas[pas_tension]);
    else if(page==CONFIRMATION)oui=d>0;
    else {int max=page==ACCUEIL?5:page==DETAIL?8:page==MEMOIRE?5:page==SORTIE?3:page==DIGITAL?6:2;curseur=borne(curseur+d,0,max);}
    sale=true;
}
static void apercu(const signal_config_t *s) {
    texte(0,"APERCU THEORIQUE");texte(1,"PAS UNE MESURE VOUT");
    int precedent=45;
    for(int x=0;x<128;x++) {
        double phase=(x%64)/64.0,v;
        if(s->forme==SIGNAL_SINUS)v=sin(phase*6.283185307179586);
        else if(s->forme==SIGNAL_TRIANGLE)v=1.0-4.0*fabs(phase-0.5);
        else v=phase<0.5?1.0:-1.0;
        int y=45-(int)(v*14);if(x)oled_segment(x-1,precedent,x,y);precedent=y;
    }
}
static void afficher(void) {
    const integration_statut_t *st=integration_statut();
    index_signal=borne(index_signal,0,(int)signaux_nombre()-1);
    const signal_config_t *s=signaux_lire((size_t)index_signal);
    char b[64],v[24];oled_effacer();defilement_necessaire=false;
    switch(page) {
    case ACCUEIL:liste("GENERATEUR / EQ1",curseur,6,ligne_accueil);break;
    case LISTE:
        snprintf(b,sizeof(b),"SIGNAUX %d/%u",index_signal+1,(unsigned)signaux_nombre());
        liste(b,index_signal,(int)signaux_nombre(),ligne_signal);break;
    case DETAIL:
        snprintf(b,sizeof(b),"SIGNAL %d / RAM",index_signal+1);
        liste(b,curseur,9,ligne_detail);break;
    case EDITION:
        texte(0,"BROUILLON SIG %d",index_signal+1);texte(2,"TYPE %s",signaux_type_court(brouillon.forme));
        texte(3,"AMPL %"PRId32" MVPP",brouillon.amplitude_mvpp);texte(4,"OFFS %"PRId32" MV",brouillon.offset_mv);
        texte(5,"FREQ %"PRIu32" HZ",brouillon.frequence_hz);texte(7,"CLIC OK / LONG ANNULE");break;
    case APERCU:apercu(s);break;
    case SORTIE:liste("SORTIE : COMMANDE",curseur,4,ligne_sortie);break;
    case SORTIE_INFO:
        texte(0,"ETAT SORTIE / CMD");texte(1,"%s",integration_nom_etat());
        texte(2,"SELECTION S%d",st->selection+1);
        if(st->derniere_valide){
            signaux_formater_frequence(v,sizeof(v),st->derniere.frequence_hz);
            texte(3,"CMD %s %s",signaux_type_court(st->derniere.forme),v);
            texte(4,"AMPL %"PRId32" MVPP",st->derniere.amplitude_mvpp);
            texte(5,"OFFS %"PRId32" MV",st->derniere.offset_mv);
        }else texte(3,"AUCUNE CMD COMPLETE");
        texte(6,"OFF != VOUT NUL");texte(7,"CLIC RET / LONG OFF");break;
    case MEMOIRE:liste("MEMOIRE / CALIBRATION",curseur,6,ligne_memoire);break;
    case MEMOIRE_INFO:
        texte(0,"ETAT RAM / PROFIL");texte(1,"%s",st->modifie?"RAM NON SAUVEE":"RAM SAUVEE");
        texte(2,"%s",st->sauvegarde_en_cours?"SAUVEGARDE EN COURS":gf_result_nom(st->sauvegarde));
        texte(3,"PROFIL %s",calibration_valider(integration_calibration())==GF_OK?"APPROUVE":"ABSENT");
        texte(4,"REV RAM %"PRIu32,st->revision);texte(5,"REV SAUV %"PRIu32,st->revision_sauvee);
        texte(7,"CLIC RET / LONG OFF");break;
    case DIAG: {
        const usb_stats_t *u=usb_service_statistiques();texte(0,"DIAGNOSTIC %d/3",curseur+1);
        if(curseur==0){texte(1,"I2C %d DDS %d",integration_i2c_disponible(),integration_ad9833_disponible());
            texte(2,"EPOT %d EEPROM %d",integration_epot_disponible(),integration_memoire_disponible());
            texte(3,"OLED ERREURS %"PRIu32,oled_erreurs());texte(4,"ENC PERTES %"PRIu32,encodeur_pertes());
            texte(5,"CHARGE %s",gf_result_nom(st->chargement));texte(6,"%s",st->etape);}
        else if(curseur==1){texte(1,"USB %s",usb_transport_disponible()?"PILOTE OK":"ABSENT");texte(2,"$1?# UN / $?# TOUS");
            texte(3,"TRAMES %"PRIu32,u->trames);texte(4,"INVALIDES %"PRIu32,u->invalides);texte(5,"TX ABANDONS %"PRIu32,u->abandons_tx);
            texte(6,"$STATE?# ETAT / RAM");}
        else {texte(1,"REV RAM %"PRIu32,st->revision);texte(2,"REV SAUV %"PRIu32,st->revision_sauvee);
            texte(3,"SELECTION %d",st->selection+1);texte(4,"%s S%d",integration_nom_etat(),st->selection+1);texte(5,"%s",gf_result_nom(st->erreur));
            texte(6,"ARRET %s",gf_result_nom(st->erreur_arret));}
        texte(7,"CLIC SUITE / LONG OFF");break;
    }
    case DIGITAL:liste("PWM OPTIONNEL 3.3V",curseur,7,ligne_digital);break;
    case DIGITAL_INFO:
        texte(0,"ETAT NUMERIQUE %d",canal+1);
        texte(2,"%s",integration_sortie_digitale_disponible((unsigned)canal)?"CANAL CONFIGURE":"CANAL NON CONFIGURE");
        texte(4,"%s",!integration_digital_statut((unsigned)canal)->connu?"ETAT NON CONFIRME":(integration_digital_statut((unsigned)canal)->actif?"COMMANDE ON":"COMMANDE OFF"));
        texte(7,"CLIC RET / LONG OFF");break;
    case PAS:liste("CLIC CHANGE LE PAS",curseur,3,ligne_pas);break;
    case CONFIRMATION:
        texte(0,"CONFIRMER ACTION ?");texte(2,"%s",confirmation==1?"ACTIVER SORTIE":confirmation==2?"SUPPRIMER SIGNAL":confirmation==3?"PERDRE RAM / CHARGER":confirmation==4?"REMPLACER PRESETS":"EFFACER CALIBRATION");
        oled_ligne(4,"NON",!oui);oled_ligne(5,"OUI",oui);texte(7,"LONG : ANNULER / OFF");break;
    case MESSAGE:
        texte(0,"RESULTAT COMMANDE");texte(2,"%s",gf_result_nom(dernier_resultat));texte(4,"%s",st->etape);
        texte(6,"%s",st->modifie?"RAM NON SAUVEE":"RAM SAUVEE");texte(7,"CLIC : RETOUR");break;
    }
}
void menu_periodique(uint32_t t) {
    now=t;
    if(page==MESSAGE && (int32_t)(now-expiration)>=0)aller(retour_message);
    if(action_en_attente){dernier_evenement=now;derniere_animation=now;action_en_attente=false;}
    if(defilement_necessaire && (uint32_t)(now-dernier_evenement)>900U
       && (uint32_t)(now-derniere_animation)>=350U){derniere_animation=now;sale=true;}
    if(sale || (uint32_t)(now-dernier_dessin)>=250){afficher();dernier_dessin=now;sale=false;}
}

/* Navigation, rendu et commandes centralisees. Pas de pilote dans le menu. */
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
typedef enum {ACCUEIL,LISTE,DETAIL,EDITION,APERCU,SORTIE,MEMOIRE,DIAG,DIGITAL,PAS,CONFIRMATION,MESSAGE} page_t;
static page_t page,retour_message,retour_confirmation;
static int curseur,index_signal,champ,confirmation,canal;
static bool oui,sale;
static signal_config_t brouillon;
static uint32_t revision_brouillon,now,dernier_dessin,expiration;
static gf_result_t dernier_resultat;
static uint32_t freq_dig[2]={1000,1000};
static uint16_t duty_dig[2]={500,500};
static const uint32_t pas[]={1,10,100,1000,10000,100000};
static unsigned pas_freq=3,pas_tension=2;
static const char *const accueil[]={"SIGNAUX","SORTIE / ARRET","MEMOIRE / PROFIL","DIAGNOSTIC / USB","NUMERIQUES","PAS DE REGLAGE"};
static int borne(int x,int lo,int hi){return x<lo?lo:(x>hi?hi:x);}
static void aller(page_t p){page=p;curseur=0;sale=true;}
static void texte(int ligne,const char *format,...) {
    char b[64];va_list a;va_start(a,format);vsnprintf(b,sizeof(b),format,a);va_end(a);oled_ligne(ligne,b,false);
}
static void item(int n,const char *p){oled_ligne(n+1,p,curseur==n);}
static void message(gf_result_t r,page_t retour){dernier_resultat=r;retour_message=retour;expiration=now+1800;aller(MESSAGE);}
static void confirmer(int action,page_t retour){confirmation=action;retour_confirmation=retour;oui=false;aller(CONFIRMATION);}
static void retour(void) {
    if(page==EDITION || page==APERCU)aller(DETAIL);
    else if(page==DETAIL)aller(LISTE);
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
        else aller(ACCUEIL);
        break;
    case MEMOIRE:
        if(curseur==0)message(integration_sauver(),MEMOIRE);
        else if(curseur==1)confirmer(3,MEMOIRE);
        else if(curseur==2)confirmer(4,MEMOIRE);
        else if(curseur==3)confirmer(5,MEMOIRE);
        else aller(ACCUEIL);
        break;
    case DIAG:curseur=(curseur+1)%3;break;
    case DIGITAL:
        if(curseur==0)canal=1-canal;
        else if(curseur==1){freq_dig[canal]=freq_dig[canal]>=100000?10:freq_dig[canal]*10;}
        else if(curseur==2)duty_dig[canal]=(uint16_t)((duty_dig[canal]+100)%1100);
        else if(curseur==3)message(integration_commander_digital((unsigned)canal,freq_dig[canal],duty_dig[canal],true),DIGITAL);
        else if(curseur==4)message(integration_commander_digital((unsigned)canal,0,0,false),DIGITAL);
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
    }
}
void menu_initialiser(void){page=ACCUEIL;curseur=0;index_signal=0;canal=0;sale=true;now=0;dernier_dessin=0;}
void menu_traiter(evenement_ui_t e) {
    if(e==UI_AUCUN)return;
    if(e==UI_APPUI_LONG){(void)integration_arreter();retour();return;}
    if(e==UI_CLIC){valider();sale=true;return;}
    int d=e==UI_ROTATION_PLUS?1:-1;
    if(page==LISTE)index_signal=borne(index_signal+d,0,(int)signaux_nombre()-1);
    else if(page==EDITION)signaux_variation(&brouillon,(signal_champ_t)champ,d,champ==CHAMP_FREQUENCE?pas[pas_freq]:pas[pas_tension]);
    else if(page==CONFIRMATION)oui=d>0;
    else {int max=page==ACCUEIL?5:page==DETAIL?8:page==MEMOIRE?4:page==SORTIE?2:page==DIGITAL?5:2;curseur=borne(curseur+d,0,max);}
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
    char b[64],v[24];oled_effacer();
    switch(page) {
    case ACCUEIL:
        texte(0,"GENERATEUR / EQ1");for(int i=0;i<6;i++)item(i,accueil[i]);
        texte(7,"%s %s",st->modifie?"RAM*":"EEPROM",st->etat==SYS_ACTIF_COMMANDE?"ON CMD":(st->etat==SYS_DEFAUT || st->etat==SYS_NON_INITIALISE?"INCERTAIN":"OFF CMD"));break;
    case LISTE: {
        texte(0,"SIGNAUX %d/%u",index_signal+1,(unsigned)signaux_nombre());
        int debut=index_signal<6?0:index_signal-5;
        for(int i=0;i<6 && debut+i<(int)signaux_nombre();i++) {
            const signal_config_t *p=signaux_lire((size_t)(debut+i));
            signaux_formater_frequence(v,sizeof(v),p->frequence_hz);
            snprintf(b,sizeof(b),"%d %s %s",debut+i+1,signaux_type_court(p->forme),v);
            oled_ligne(i+1,b,debut+i==index_signal);
        }texte(7,"LONG : ARRET / RETOUR");break;
    }
    case DETAIL: {
        texte(0,"SIGNAL %d / RAM",index_signal+1);
        int debut=curseur<6?0:curseur-5;
        for(int i=0;i<6 && debut+i<9;i++) {
            int n=debut+i;
            if(n==0)snprintf(b,sizeof(b),"SELECTIONNER");
            else if(n==1)snprintf(b,sizeof(b),"TYPE %s",signaux_type_court(s->forme));
            else if(n==2)snprintf(b,sizeof(b),"AMPL %"PRId32" MVPP",s->amplitude_mvpp);
            else if(n==3)snprintf(b,sizeof(b),"OFFS %"PRId32" MV",s->offset_mv);
            else if(n==4){signaux_formater_frequence(v,sizeof(v),s->frequence_hz);snprintf(b,sizeof(b),"FREQ %s",v);}
            else snprintf(b,sizeof(b),"%s",n==5?"APERCU THEORIQUE":n==6?"DUPLIQUER":n==7?"SUPPRIMER":"RETOUR");
            oled_ligne(i+1,b,n==curseur);
        }texte(7,"EDITER NE GENERE PAS");break;
    }
    case EDITION:
        texte(0,"BROUILLON SIG %d",index_signal+1);texte(2,"TYPE %s",signaux_type_court(brouillon.forme));
        texte(3,"AMPL %"PRId32" MVPP",brouillon.amplitude_mvpp);texte(4,"OFFS %"PRId32" MV",brouillon.offset_mv);
        texte(5,"FREQ %"PRIu32" HZ",brouillon.frequence_hz);texte(7,"CLIC OK / LONG ANNULE");break;
    case APERCU:apercu(s);break;
    case SORTIE:
        texte(0,"SORTIE : COMMANDE");item(0,"ARRETER TOUT");item(1,"ACTIVER SELECTION");item(2,"RETOUR");
        texte(4,"%s S%d",integration_nom_etat(),st->selection+1);
        if(st->derniere_valide){signaux_formater_frequence(v,sizeof(v),st->derniere.frequence_hz);texte(5,"CMD %s %s",signaux_type_court(st->derniere.forme),v);}
        if(st->derniere_valide)texte(6,"A%"PRId32" O%"PRId32" MV",st->derniere.amplitude_mvpp,st->derniere.offset_mv);
        texte(7,"OFF != VOUT NUL");break;
    case MEMOIRE:
        texte(0,"MEMOIRE / CALIBRATION");item(0,"SAUVER MAINTENANT");item(1,"RECHARGER EEPROM");item(2,"PRESETS USINE");item(3,"EFFACER PROFIL");item(4,"RETOUR");
        texte(6,"%s %s",st->modifie?"RAM*":"SAUVE",st->sauvegarde_en_cours?"EN COURS":gf_result_nom(st->sauvegarde));
        texte(7,"PROFIL %s",calibration_valider(integration_calibration())==GF_OK?"APPROUVE":"ABSENT");break;
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
    case DIGITAL:
        texte(0,"PWM OPTIONNEL 3.3V");snprintf(b,sizeof(b),"CANAL %d : %s",canal+1,integration_sortie_digitale_disponible((unsigned)canal)?"EQUIPE":"NON CONFIG");item(0,b);
        snprintf(b,sizeof(b),"FREQ %"PRIu32" HZ",freq_dig[canal]);item(1,b);
        snprintf(b,sizeof(b),"RAPPORT %u/1000",duty_dig[canal]);item(2,b);item(3,"APPLIQUER / ON");item(4,"ARRETER / OFF");item(5,"RETOUR");
        texte(7,"%s",!integration_digital_statut((unsigned)canal)->connu?"ETAT NON CONFIRME":(integration_digital_statut((unsigned)canal)->actif?"COMMANDE ON":"COMMANDE OFF"));break;
    case PAS:
        texte(0,"CLIC CHANGE LE PAS");snprintf(b,sizeof(b),"FREQ %"PRIu32" HZ",pas[pas_freq]);item(0,b);
        snprintf(b,sizeof(b),"TENSION %"PRIu32" MV",pas[pas_tension]);item(1,b);item(2,"RETOUR");break;
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
    if(sale || (uint32_t)(now-dernier_dessin)>=250){afficher();dernier_dessin=now;sale=false;}
}

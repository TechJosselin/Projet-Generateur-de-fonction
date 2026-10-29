/* Interface locale en lecture seule : parcourir, activer, arreter.
 * Les configurations et leur persistance sont gerees exclusivement via USB.
 * Rendu trois lignes conserve depuis feature/menu-oled-3-lignes. */
#include "menu.h"
#include "integration.h"
#include "oled.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

typedef enum {LISTE,DETAIL,MESSAGE} page_t;
static page_t page;
static int index_signal,premiere_ligne;
static bool sale,fenetre_valide,action_en_attente,defilement_necessaire;
static bool erreur_arret;
static uint32_t now,dernier_dessin,dernier_evenement,derniere_animation;
static gf_result_t dernier_resultat;

static int borne(int x,int lo,int hi){return x<lo?lo:(x>hi?hi:x);}
static void recadrer(void) {
    size_t n=signaux_nombre();
    index_signal=n?borne(index_signal,0,(int)n-1):0;
}
static void aller(page_t p) {
    if(page!=p)fenetre_valide=false;
    page=p;sale=true;action_en_attente=true;defilement_necessaire=false;
}
static void texte(int ligne,const char *format,...) {
    char b[64];va_list a;va_start(a,format);vsnprintf(b,sizeof(b),format,a);va_end(a);
    oled_ligne(ligne,b,false);
}
static void bandeau(const char *titre) {
    oled_ligne(0,titre,true);oled_segment(0,10,127,10);
}
static void ligne_signal(int i,char *b,size_t n) {
    const signal_config_t *s=signaux_lire((size_t)i);
    if(!s){*b=0;return;}
    char a[24],f[24];
    signaux_formater_tension(a,sizeof(a),s->amplitude_mvpp,false);
    signaux_formater_frequence(f,sizeof(f),s->frequence_hz);
    snprintf(b,n,"%02d %s %s %s",i+1,signaux_type_court(s->forme),a,f);
}
/* Bandeau fixe et trois grandes lignes ; seule la ligne pointee defile.
 * La rotation deplace le pointeur, jamais la configuration ni la sortie. */
static void liste(const char *titre) {
    bandeau(titre);
    int total=(int)signaux_nombre();
    if(total<=0){texte(3,"AUCUN SIGNAL / USB");return;}
    int max_premiere=total>3?total-3:0;
    if(!fenetre_valide) {
        premiere_ligne=borne(index_signal-1,0,max_premiere);fenetre_valide=true;
    }
    if(index_signal<premiere_ligne)premiere_ligne=index_signal;
    if(index_signal>=premiere_ligne+3)premiere_ligne=index_signal-2;
    premiere_ligne=borne(premiere_ligne,0,max_premiere);
    for(int i=0;i<3 && premiere_ligne+i<total;i++) {
        char complet[64]={0},visible[OLED_GRAND_NB_CARACTERES+1];
        ligne_signal(premiere_ligne+i,complet,sizeof(complet));
        bool choisi=premiere_ligne+i==index_signal;
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
static void message(gf_result_t r,bool arret) {
    dernier_resultat=r;erreur_arret=arret;aller(MESSAGE);
}
static void valider(void) {
    if(page!=LISTE){aller(LISTE);return;}
    /* Une seule validation locale : selection ET activation coordonnee.
     * Ne pas activer l'ancienne selection si choisir() a echoue. */
    gf_result_t r=integration_choisir((size_t)index_signal);
    if(r==GF_OK)r=integration_activer();
    if(r==GF_OK)aller(DETAIL);else message(r,false);
}
void menu_initialiser(void) {
    page=LISTE;index_signal=0;premiere_ligne=0;sale=true;now=0;dernier_dessin=0;
    fenetre_valide=false;action_en_attente=true;defilement_necessaire=false;
    dernier_evenement=0;derniere_animation=0;erreur_arret=false;dernier_resultat=GF_OK;
}
void menu_traiter(evenement_ui_t e) {
    if(e!=UI_ROTATION_PLUS && e!=UI_ROTATION_MOINS && e!=UI_CLIC && e!=UI_APPUI_LONG)return;
    recadrer();action_en_attente=true;
    if(e==UI_APPUI_LONG) {
        gf_result_t r=integration_arreter();
        if(r==GF_OK)aller(LISTE);else message(r,true);
        return;
    }
    if(e==UI_CLIC){valider();sale=true;return;}
    /* Depuis la fiche, tourner revient directement au choix suivant/precedent. */
    if(page!=LISTE)aller(LISTE);
    int d=e==UI_ROTATION_PLUS?1:-1;
    if(signaux_nombre())index_signal=borne(index_signal+d,0,(int)signaux_nombre()-1);
    sale=true;
}
static void afficher(void) {
    const integration_statut_t *st=integration_statut();
    recadrer();
    const signal_config_t *s=signaux_lire((size_t)index_signal);
    char b[64],v[24];oled_effacer();defilement_necessaire=false;
    bool actif=st->etat==SYS_ACTIF_COMMANDE && st->commande_courante;
    switch(page) {
    case LISTE:
        if(st->etat==SYS_DEFAUT)
            snprintf(b,sizeof(b),"SIG %d/%u DEFAUT",index_signal+1,(unsigned)signaux_nombre());
        else if(actif)
            snprintf(b,sizeof(b),"SIG %d/%u ON S%d",index_signal+1,(unsigned)signaux_nombre(),st->selection+1);
        else snprintf(b,sizeof(b),"SIG %d/%u OFF",index_signal+1,(unsigned)signaux_nombre());
        liste(b);break;
    case DETAIL:
        if(!s){aller(LISTE);liste("AUCUN SIGNAL / USB");break;}
        snprintf(b,sizeof(b),"S%d %s / %s",index_signal+1,signaux_type_court(s->forme),
                 st->etat==SYS_DEFAUT?"DEFAUT":(actif && st->selection==index_signal?"ON":"OFF"));
        bandeau(b);
        /* F, A (crete-a-crete), O : valeurs exactes, aucune action d'edition. */
        signaux_formater_frequence(v,sizeof(v),s->frequence_hz);
        snprintf(b,sizeof(b),"F %s",v);oled_texte_grand(4,14,b);
        signaux_formater_tension(v,sizeof(v),s->amplitude_mvpp,false);
        snprintf(b,sizeof(b),"A %spp",v);oled_texte_grand(4,31,b);
        signaux_formater_tension(v,sizeof(v),s->offset_mv,true);
        snprintf(b,sizeof(b),"O %s",v);oled_texte_grand(4,48,b);
        break;
    case MESSAGE:
        texte(0,"%s",erreur_arret?"ARRET NON CONFIRME":"SORTIE NON ACTIVEE");
        texte(2,"%s",gf_result_nom(dernier_resultat));texte(4,"%s",st->etape);
        texte(6,"REGLAGES : USB SEUL");texte(7,"CLIC LISTE / LONG OFF");break;
    }
}
void menu_periodique(uint32_t t) {
    now=t;
    if(action_en_attente){dernier_evenement=now;derniere_animation=now;action_en_attente=false;}
    if(defilement_necessaire && (uint32_t)(now-dernier_evenement)>900U
       && (uint32_t)(now-derniere_animation)>=350U){derniere_animation=now;sale=true;}
    if(sale || (uint32_t)(now-dernier_dessin)>=250){afficher();dernier_dessin=now;sale=false;}
}

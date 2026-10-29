#include "usb_service.h"
#include "integration.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
static uint8_t tx[USB_TX_CAPACITE],cal_image[CAL_IMAGE_SIZE];
static char rx[USB_TRAME_MAX];
static size_t debut,utilise,rx_n;
static uint32_t rx_t,tx_t,now,cal_t,cal_revision;
static uint16_t cal_masque;
static bool cal_en_cours,reset_transport;
static usb_stats_t stats;
void usb_service_initialiser(void) {
    debut=utilise=rx_n=0;rx_t=tx_t=now=cal_t=cal_revision=0;
    cal_masque=0;cal_en_cours=reset_transport=false;memset(&stats,0,sizeof(stats));
}
const usb_stats_t *usb_service_statistiques(void){return &stats;}
size_t usb_service_espace_tx(void){return USB_TX_CAPACITE-utilise;}
static void emettre(const char *s) {
    size_t n=strlen(s);if(n>usb_service_espace_tx()){stats.abandons_tx++;return;}
    if(!utilise)tx_t=now;
    for(size_t i=0;i<n;i++)tx[(debut+utilise+i)%USB_TX_CAPACITE]=(uint8_t)s[i];
    utilise+=n;
}
static void erreur(gf_result_t r) {
    char b[64];snprintf(b,sizeof(b),"$ERR:%s#",gf_result_nom(r));emettre(b);
}
static void ok(gf_result_t r,const char *action) {
    if(r!=GF_OK){erreur(r);return;}
    char b[80];snprintf(b,sizeof(b),"$OK:%s:%"PRIu32"#",action,integration_statut()->revision);emettre(b);
}
static void executer(const usb_commande_t *c) {
    char b[192];gf_result_t r=GF_OK;
    /* Reserve une reponse avant toute mutation, pas d'action silencieuse. */
    if(usb_service_espace_tx()<sizeof(b)){erreur(GF_OCCUPE);return;}
    switch(c->action) {
    case USB_ECRIRE:ok(integration_modifier(c->index,c->signal,0),"RAM");break;
    case USB_LIRE:
        if(usb_proto_formater_signal(c->index,b,sizeof(b)))emettre(b);else erreur(GF_INDEX);break;
    case USB_TOUT:
        if(usb_service_espace_tx()<signaux_nombre()*64U){erreur(GF_OCCUPE);break;}
        /* Propriete unique du modele : la suite est capturee avant de rendre la main. */
        for(size_t i=0;i<signaux_nombre();i++) {
            if(!usb_proto_formater_signal(i,b,sizeof(b))){erreur(GF_ARGUMENT);break;}
            emettre(b);
        }break;
    case USB_ETAT: {
        const integration_statut_t *s=integration_statut();
        snprintf(b,sizeof(b),"$STATE:%s:%d:%u:%"PRIu32":%"PRIu32":%u:%u:%s:%s:%s#",
            integration_nom_etat(),s->selection+1,(unsigned)signaux_nombre(),s->revision,s->revision_sauvee,
            s->modifie?1U:0U,s->sauvegarde_en_cours?1U:0U,gf_result_nom(s->sauvegarde),
            gf_result_nom(s->erreur),gf_result_nom(s->erreur_arret));emettre(b);break;
    }
    case USB_CHOISIR:ok(integration_choisir(c->index),"SELECT");break;
    case USB_ON:ok(integration_activer(),"ON_COMMANDE");break;
    case USB_OFF:ok(integration_arreter(),"OFF_COMMANDE");break;
    case USB_SAUVER:ok(integration_sauver(),"SAVE_DEMANDE");break;
    case USB_CHARGER:ok(integration_charger(),"LOAD");break;
    case USB_USINE:ok(integration_usine(),"RAM");break;
    case USB_DUPLIQUER:ok(integration_dupliquer(c->index,NULL),"RAM");break;
    case USB_SUPPRIMER:ok(integration_supprimer(c->index),"RAM");break;
    case USB_DIGITAL:ok(integration_commander_digital((unsigned)c->index,c->frequence,c->rapport,c->actif),"DIGITAL");break;
    case USB_DIG_INFOS:
        for(unsigned canal=0;canal<2;canal++) {
            const digital_statut_t *d=integration_digital_statut(canal);
            snprintf(b,sizeof(b),"$DIG:%u:%u:%u:%u:%"PRIu32":%u:%s#",canal+1,
                integration_sortie_digitale_disponible(canal)?1U:0U,d->connu?1U:0U,d->actif?1U:0U,
                d->frequence_hz,(unsigned)d->rapport_pm,gf_result_nom(d->erreur));emettre(b);
        }break;
    case USB_CAL_BEGIN:
        memset(cal_image,0,sizeof(cal_image));cal_masque=0;cal_en_cours=true;cal_t=now;
        cal_revision=integration_statut()->revision;ok(GF_OK,"CAL_BEGIN");break;
    case USB_CAL_DATA:
        if(!cal_en_cours){erreur(GF_CONFLIT);break;}
        memcpy(cal_image+c->position,c->donnees,32);cal_masque|=(uint16_t)(1U<<(c->position/32));
        cal_t=now;ok(GF_OK,"CAL_DATA");break;
    case USB_CAL_COMMIT:
        if(!cal_en_cours || cal_masque!=UINT16_MAX || cal_revision!=integration_statut()->revision)r=GF_CONFLIT;
        else r=integration_importer_calibration(cal_image);
        cal_en_cours=false;ok(r,"RAM");break;
    case USB_CAL_ABORT:cal_en_cours=false;ok(GF_OK,"CAL_ABORT");break;
    case USB_CAL_CLEAR:cal_en_cours=false;ok(integration_effacer_calibration(),"RAM");break;
    case USB_CAL_GET: {
        if(cal_en_cours){erreur(GF_OCCUPE);break;}
        if(usb_service_espace_tx()<2048){erreur(GF_OCCUPE);break;}
        r=calibration_encoder(integration_calibration(),cal_image);
        if(r!=GF_OK){erreur(r);break;}
        static const char hex[]="0123456789ABCDEF";
        for(unsigned pos=0;pos<CAL_IMAGE_SIZE;pos+=32) {
            int n=snprintf(b,sizeof(b),"$CAL:DATA:%u:",pos);
            for(unsigned i=0;i<32;i++){b[n++]=hex[cal_image[pos+i]>>4];b[n++]=hex[cal_image[pos+i]&15];}
            b[n++]='#';b[n]=0;emettre(b);
        }
        emettre("$CAL:END#");break;
    }
    }
}
void usb_service_octet(uint8_t c,uint32_t t) {
    now=t;
    if(c=='$'){rx[0]='$';rx_n=1;rx_t=t;return;}
    if(!rx_n)return;
    if(c<32 || c>126){rx_n=0;stats.invalides++;erreur(GF_ARGUMENT);return;}
    if(rx_n>=sizeof(rx)-1){rx_n=0;stats.depassements++;erreur(GF_CAPACITE);return;}
    rx[rx_n++]=(char)c;rx_t=t;
    if(c=='#') {
        rx[rx_n]=0;usb_commande_t commande;
        gf_result_t r=usb_proto_decoder(rx,&commande);rx_n=0;stats.trames++;
        if(r!=GF_OK){stats.invalides++;erreur(r);}else executer(&commande);
    }
}
void usb_service_periodique(uint32_t t) {
    now=t;
    if(rx_n && (uint32_t)(t-rx_t)>=1000){rx_n=0;stats.abandons_rx++;erreur(GF_TIMEOUT);}
    if(cal_en_cours && (uint32_t)(t-cal_t)>=30000)cal_en_cours=false;
    if(utilise && (uint32_t)(t-tx_t)>=2000) {
        utilise=debut=rx_n=0;stats.abandons_tx++;reset_transport=true;
        /* Le transport vide aussi ses propres buffers pour eviter des queues stale. */
    }
}
const uint8_t *usb_service_tx(size_t *n) {
    if(!n)return NULL;
    *n=utilise;if(*n>USB_TX_CAPACITE-debut)*n=USB_TX_CAPACITE-debut;
    if(*n>64)*n=64;
    return *n?tx+debut:NULL;
}
void usb_service_tx_consomme(size_t n,uint32_t t) {
    if(n>utilise)n=utilise;
    debut=(debut+n)%USB_TX_CAPACITE;utilise-=n;
    if(n){tx_t=t;stats.octets_tx+=(uint32_t)n;}
}
bool usb_service_reinitialisation_transport(void) {bool b=reset_transport;reset_transport=false;return b;}

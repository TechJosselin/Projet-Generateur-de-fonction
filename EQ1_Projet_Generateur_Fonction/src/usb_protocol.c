#include "usb_protocol.h"
#include "signaux.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool lire_entier(const char *texte,int64_t min,int64_t max,int64_t *sortie){
    if(!texte || !*texte || !sortie)return false;
    errno=0;
    char *fin=NULL;
    long long v=strtoll(texte,&fin,10);
    if(errno || fin==texte || *fin || v<min || v>max)return false;
    *sortie=v;return true;
}
static bool numero_signal(const char *texte,size_t *index){
    int64_t n;
    if(!lire_entier(texte,1,SIGNAL_MAX,&n))return false;
    *index=(size_t)(n-1);
    return true;
}
static void mettre_erreur(char *dst,size_t n,const char *cause){
    if(dst && n)snprintf(dst,n,"ERR:%s",cause);
}
bool usb_proto_formater_signal(size_t index,char *reponse,size_t taille){
    const signal_config_t *s=signaux_lire(index);
    if(!s || !reponse || !taille)return false;
    int n=snprintf(reponse,taille,"$%u:%s:%"PRId32":%+"PRId32":%"PRIu32"#",
                    (unsigned)(index+1),signaux_type_court(s->forme),
                    s->amplitude_mvpp,s->offset_mv,s->frequence_hz);
    return n>=0 && (size_t)n<taille;
}
usb_proto_resultat_t usb_proto_traiter(const char *trame,char *rep,size_t taille){
    if(!trame || !rep || taille==0)return USB_PROTO_ERREUR;
    rep[0]=0;
    size_t n=strlen(trame);
    if(n<3 || n>63 || trame[0]!='$' || trame[n-1]!='#'){
        mettre_erreur(rep,taille,"TRAME");return USB_PROTO_ERREUR;
    }
    if(strcmp(trame,"$?#")==0){
        snprintf(rep,taille,"OK:LISTE");
        return USB_PROTO_LECTURE_TOUT;
    }
    char corps[64];
    memcpy(corps,trame+1,n-2);corps[n-2]=0;
    char *question=strchr(corps,'?');
    if(question){
        if(question[1] || strchr(corps,':')){
            mettre_erreur(rep,taille,"SYNTAXE");return USB_PROTO_ERREUR;
        }
        *question=0;
        size_t index;
        if(!numero_signal(corps,&index) || index>=signaux_nombre() ||
           !usb_proto_formater_signal(index,rep,taille)){
            mettre_erreur(rep,taille,"NUMERO");return USB_PROTO_ERREUR;
        }
        return USB_PROTO_LECTURE_UN;
    }
    char *champs[5]={0};
    champs[0]=corps;
    int idx=1;
    for(char *p=corps;*p;p++){
        if(*p==':'){
            if(idx>=5){mettre_erreur(rep,taille,"CHAMPS");return USB_PROTO_ERREUR;}
            *p=0;champs[idx++]=p+1;
        }
    }
    if(idx!=5){mettre_erreur(rep,taille,"CHAMPS");return USB_PROTO_ERREUR;}
    size_t index;
    if(!numero_signal(champs[0],&index) || index>=signaux_nombre()){
        mettre_erreur(rep,taille,"NUMERO");return USB_PROTO_ERREUR;
    }
    signal_config_t s={0};
    if(!strcmp(champs[1],"SIN"))s.forme=SIGNAL_SINUS;
    else if(!strcmp(champs[1],"TRI"))s.forme=SIGNAL_TRIANGLE;
    else if(!strcmp(champs[1],"CAR"))s.forme=SIGNAL_CARRE;
    else{mettre_erreur(rep,taille,"TYPE");return USB_PROTO_ERREUR;}
    int64_t amplitude,offset,frequence;
    if(strlen(champs[2])>5 || strlen(champs[3])>6 || strlen(champs[4])>8 ||
       !lire_entier(champs[2],SIGNAL_AMPL_MIN_MVPP,SIGNAL_AMPL_MAX_MVPP,&amplitude) ||
       !lire_entier(champs[3],SIGNAL_OFFSET_MIN_MV,SIGNAL_OFFSET_MAX_MV,&offset) ||
       !lire_entier(champs[4],SIGNAL_FREQ_MIN_HZ,SIGNAL_FREQ_MAX_HZ,&frequence)){
        mettre_erreur(rep,taille,"VALEUR");return USB_PROTO_ERREUR;
    }
    s.amplitude_mvpp=(int32_t)amplitude;
    s.offset_mv=(int32_t)offset;
    s.frequence_hz=(uint32_t)frequence;
    if(!signaux_modifier(index,s)){
        mettre_erreur(rep,taille,"VALIDATION");return USB_PROTO_ERREUR;
    }
    snprintf(rep,taille,"OK:RAM");
    return USB_PROTO_ECRITURE_RAM;
}

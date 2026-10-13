#include "signaux.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static signal_config_t catalogue[SIGNAL_MAX];
static bool changes[SIGNAL_MAX];
static size_t total;
static bool catalogue_change;

/* Les dix valeurs du cahier des charges, en mVpp / mV / Hz. */
static const signal_config_t usine[SIGNAL_INIT_COUNT]={
    {SIGNAL_SINUS,    1000,     0,    10000},
    {SIGNAL_SINUS,    5000,     0,  1000000},
    {SIGNAL_SINUS,    2000,  3000,     1000},
    {SIGNAL_SINUS,     200, 10000,      500},
    {SIGNAL_TRIANGLE,20000,     0,     5000},
    {SIGNAL_TRIANGLE, 4000,     0,      100},
    {SIGNAL_TRIANGLE, 5000, -5000,   200000},
    {SIGNAL_CARRE,   10000,     0,       50},
    {SIGNAL_CARRE,    1000,     0,      300},
    {SIGNAL_CARRE,    4000,  6000,     1000}
};

void signaux_initialiser(void){
    memset(catalogue,0,sizeof(catalogue));
    memset(changes,0,sizeof(changes));
    memcpy(catalogue,usine,sizeof(usine));
    total=SIGNAL_INIT_COUNT;
    catalogue_change=false;
}
void signaux_retablir_usine(void){
    signaux_initialiser();
    catalogue_change=true;
}
size_t signaux_nombre(void){return total;}
const signal_config_t *signaux_lire(size_t i){return i<total?&catalogue[i]:NULL;}
bool signaux_valide(signal_config_t s){
    return s.forme>=SIGNAL_SINUS && s.forme<=SIGNAL_CARRE &&
           s.amplitude_mvpp>=SIGNAL_AMPL_MIN_MVPP && s.amplitude_mvpp<=SIGNAL_AMPL_MAX_MVPP &&
           s.offset_mv>=SIGNAL_OFFSET_MIN_MV && s.offset_mv<=SIGNAL_OFFSET_MAX_MV &&
           s.frequence_hz>=SIGNAL_FREQ_MIN_HZ && s.frequence_hz<=SIGNAL_FREQ_MAX_HZ;
}
bool signaux_remplacer_ensemble(const signal_config_t *donnees,size_t n){
    if(!donnees || n==0 || n>SIGNAL_MAX)return false;
    for(size_t i=0;i<n;i++)if(!signaux_valide(donnees[i]))return false;
    memcpy(catalogue,donnees,n*sizeof(*donnees));
    if(n<SIGNAL_MAX)memset(catalogue+n,0,(SIGNAL_MAX-n)*sizeof(*donnees));
    memset(changes,0,sizeof(changes));
    total=n;
    catalogue_change=false;
    return true;
}
bool signaux_modifier(size_t i,signal_config_t s){
    if(i>=total || !signaux_valide(s))return false;
    if(catalogue[i].forme!=s.forme || catalogue[i].amplitude_mvpp!=s.amplitude_mvpp ||
       catalogue[i].offset_mv!=s.offset_mv || catalogue[i].frequence_hz!=s.frequence_hz){
        catalogue[i]=s;
        changes[i]=true;
    }
    return true;
}
int signaux_dupliquer(size_t i){
    if(i>=total || total>=SIGNAL_MAX)return -1;
    catalogue[total]=catalogue[i];
    changes[total]=true;
    catalogue_change=true;
    return (int)total++;
}
bool signaux_supprimer(size_t i){
    if(i>=total || total<=1)return false;
    for(size_t j=i;j+1<total;j++){
        catalogue[j]=catalogue[j+1];
        changes[j]=changes[j+1];
    }
    total--;
    memset(&catalogue[total],0,sizeof(catalogue[total]));
    changes[total]=false;
    catalogue_change=true;
    return true;
}
bool signaux_modifie(size_t i){return i<total && changes[i];}
size_t signaux_nombre_modifies(void){
    size_t n=0;for(size_t i=0;i<total;i++)if(changes[i])n++;return n;
}
bool signaux_modifications_en_ram(void){return catalogue_change || signaux_nombre_modifies()!=0;}
void signaux_variation(signal_config_t *s,signal_champ_t champ,int direction,uint32_t pas){
    if(!s || !direction)return;
    int sens=direction>0?1:-1;
    if(champ==CHAMP_FORME){
        int f=(int)s->forme+sens;
        s->forme=(signal_forme_t)(f<0?2:(f>2?0:f));
    }else if(champ==CHAMP_AMPLITUDE || champ==CHAMP_OFFSET){
        int64_t v=champ==CHAMP_AMPLITUDE?s->amplitude_mvpp:s->offset_mv;
        int64_t mini=champ==CHAMP_AMPLITUDE?SIGNAL_AMPL_MIN_MVPP:SIGNAL_OFFSET_MIN_MV;
        int64_t maxi=champ==CHAMP_AMPLITUDE?SIGNAL_AMPL_MAX_MVPP:SIGNAL_OFFSET_MAX_MV;
        v+=(int64_t)sens*(int64_t)(pas?pas:100);
        if(v<mini)v=mini;
        if(v>maxi)v=maxi;
        if(champ==CHAMP_AMPLITUDE)s->amplitude_mvpp=(int32_t)v;
        else s->offset_mv=(int32_t)v;
    }else if(champ==CHAMP_FREQUENCE){
        int64_t v=(int64_t)s->frequence_hz+(int64_t)sens*(int64_t)(pas?pas:1);
        if(v<SIGNAL_FREQ_MIN_HZ)v=SIGNAL_FREQ_MIN_HZ;
        if(v>SIGNAL_FREQ_MAX_HZ)v=SIGNAL_FREQ_MAX_HZ;
        s->frequence_hz=(uint32_t)v;
    }
}
const char *signaux_type_court(signal_forme_t s){
    switch(s){case SIGNAL_SINUS:return "SIN";case SIGNAL_TRIANGLE:return "TRI";
              case SIGNAL_CARRE:return "CAR";default:return "???";}
}
const char *signaux_type_long(signal_forme_t s){
    switch(s){case SIGNAL_SINUS:return "SINUS";case SIGNAL_TRIANGLE:return "TRIANGLE";
              case SIGNAL_CARRE:return "CARRE";default:return "INVALIDE";}
}
void signaux_formater_tension(char *dst,size_t n,int32_t mv,bool signe){
    if(!dst || !n)return;
    int64_t v=mv;char sign=v<0?'-':'+';if(v<0)v=-v;
    if(signe)snprintf(dst,n,"%c%"PRId64".%03"PRId64"V",sign,v/1000,v%1000);
    else snprintf(dst,n,"%"PRId64".%03"PRId64"V",v/1000,v%1000);
}
void signaux_formater_frequence(char *dst,size_t n,uint32_t hz){
    if(!dst || !n)return;
    if(hz>=1000000 && hz%1000000==0)snprintf(dst,n,"%"PRIu32"MHz",hz/1000000);
    else if(hz>=1000 && hz%1000==0)snprintf(dst,n,"%"PRIu32"kHz",hz/1000);
    else snprintf(dst,n,"%"PRIu32"Hz",hz);
}

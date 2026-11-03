#include "stockage.h"
#include <string.h>
#define MAGIC "GFN2"
#define MARQUEUR 0xA5U
#define ENTETE 32U
#define ENREGISTREMENT 13U
_Static_assert(ENTETE+SIGNAL_MAX*ENREGISTREMENT+CAL_IMAGE_SIZE<=STOCKAGE_BANQUE,"EEPROM trop petite");
enum {REPOS=0,INVALIDER,CONTROLER_INVALIDE,CORPS,HEADER,VERIFIER,VALIDER,CONTROLER_VALIDE};

void stockage_initialiser(stockage_t *s,stockage_io_t io) {
    memset(s,0,sizeof(*s));s->io=io;s->actif=-1;s->cible=-1;
}
bool stockage_occupe(const stockage_t *s){return s && s->etape!=REPOS;}
static bool plus_recent(uint32_t a,uint32_t b){uint32_t d=a-b;return d!=0 && d<UINT32_C(0x80000000);}
static gf_result_t decoder(const uint8_t *b,stockage_donnees_t *d,uint32_t *generation) {
    if(b[31]!=MARQUEUR)return GF_CRC;
    if(memcmp(b,MAGIC,4) || gf_u32(b+24)!=gf_crc32(b,24))return GF_CRC;
    if(gf_u16(b+4)!=STOCKAGE_VERSION)return GF_VERSION;
    size_t n=gf_u16(b+12),len=gf_u16(b+6);
    if(!n || n>SIGNAL_MAX || gf_u16(b+14)!=CAL_IMAGE_SIZE || len!=n*ENREGISTREMENT+CAL_IMAGE_SIZE ||
       len>STOCKAGE_BANQUE-ENTETE)return GF_CRC;
    if(gf_u32(b+20)!=gf_crc32(b+ENTETE,len))return GF_CRC;
    for(size_t i=0;i<n;i++) {
        const uint8_t *p=b+ENTETE+i*ENREGISTREMENT;
        signal_config_t v={(signal_forme_t)p[0],gf_i32(p+1),gf_i32(p+5),gf_u32(p+9)};
        if(!signaux_valide(v))return GF_CRC;
        d->signaux[i]=v;
    }
    gf_result_t r=calibration_decoder(b+ENTETE+n*ENREGISTREMENT,&d->calibration);
    if(r!=GF_OK)return r;
    d->nombre=n;*generation=gf_u32(b+8);return GF_OK;
}
gf_result_t stockage_charger(stockage_t *s,stockage_donnees_t *d) {
    if(!s || !d)return GF_ARGUMENT;
    if(stockage_occupe(s))return GF_OCCUPE;
    if(!s->io.read)return GF_ABSENT;
    stockage_donnees_t tmp;bool trouve=false,tout_vierge=true;gf_result_t erreur=GF_CRC;
    int actif=-1;uint32_t generation=0;s->alerte=GF_OK;
    for(unsigned banque=0;banque<2;banque++) {
        gf_result_t r=s->io.read(s->io.ctx,(uint16_t)(banque*STOCKAGE_BANQUE),s->image,STOCKAGE_BANQUE);
        if(r!=GF_OK){tout_vierge=false;erreur=r;s->alerte=r;continue;}
        bool vierge=true;
        for(size_t i=0;i<STOCKAGE_BANQUE;i++)if(s->image[i]!=0xFF){vierge=false;break;}
        tout_vierge=tout_vierge && vierge;
        if(vierge)continue;
        uint32_t gen=0;r=decoder(s->image,&tmp,&gen);
        if(r!=GF_OK){erreur=r;s->alerte=r;continue;}
        if(!trouve || plus_recent(gen,generation)) {
            *d=tmp;generation=gen;actif=(int)banque;trouve=true;
        }
    }
    if(trouve){s->actif=actif;s->generation=generation;return GF_OK;}
    s->actif=-1;s->generation=0;return tout_vierge?GF_VIDE:erreur;
}
gf_result_t stockage_commencer(stockage_t *s,const stockage_donnees_t *d) {
    if(!s || !d || !d->nombre || d->nombre>SIGNAL_MAX)return GF_ARGUMENT;
    if(stockage_occupe(s))return GF_OCCUPE;
    if(!s->io.read || !s->io.page_write || !s->io.ready)return GF_ABSENT;
    for(size_t i=0;i<d->nombre;i++)if(!signaux_valide(d->signaux[i]))return GF_ARGUMENT;
    uint8_t *b=s->image;memset(b,0xFF,STOCKAGE_BANQUE);memset(b,0,ENTETE);
    memcpy(b,MAGIC,4);gf_p16(b+4,STOCKAGE_VERSION);
    s->longueur=ENTETE+d->nombre*ENREGISTREMENT+CAL_IMAGE_SIZE;
    gf_p16(b+6,(uint16_t)(s->longueur-ENTETE));
    s->generation_cible=s->generation+1;gf_p32(b+8,s->generation_cible);
    gf_p16(b+12,(uint16_t)d->nombre);gf_p16(b+14,CAL_IMAGE_SIZE);
    for(size_t i=0;i<d->nombre;i++) {
        uint8_t *p=b+ENTETE+i*ENREGISTREMENT;const signal_config_t *v=&d->signaux[i];
        p[0]=(uint8_t)v->forme;gf_p32(p+1,(uint32_t)v->amplitude_mvpp);
        gf_p32(p+5,(uint32_t)v->offset_mv);gf_p32(p+9,v->frequence_hz);
    }
    gf_result_t r=calibration_encoder(&d->calibration,b+ENTETE+d->nombre*ENREGISTREMENT);
    if(r!=GF_OK)return r;
    gf_p32(b+20,gf_crc32(b+ENTETE,s->longueur-ENTETE));gf_p32(b+24,gf_crc32(b,24));
    s->cible=s->actif==0?1:0;s->position=ENTETE;s->etape=INVALIDER;s->attend=false;s->resultat=GF_OK;
    return GF_OK;
}
static void terminer(stockage_t *s,gf_result_t r) {s->resultat=r;s->etape=REPOS;s->attend=false;}
static bool ecrire(stockage_t *s,size_t offset,const uint8_t *p,size_t n,uint32_t now) {
    gf_result_t r=s->io.page_write(s->io.ctx,(uint16_t)(s->cible*STOCKAGE_BANQUE+offset),p,n);
    if(r!=GF_OK){terminer(s,r);return false;}
    s->attend=true;s->attente_depuis=now;return true;
}
void stockage_pas(stockage_t *s,uint32_t now) {
    if(!stockage_occupe(s))return;
    if(s->attend) {
        gf_result_t r=s->io.ready(s->io.ctx);
        if(r==GF_OCCUPE) {if((uint32_t)(now-s->attente_depuis)>=50)terminer(s,GF_TIMEOUT);return;}
        if(r!=GF_OK){terminer(s,r);return;}s->attend=false;
        return; /* Une operation IO maximum par iteration. */
    }
    uint8_t buf[STOCKAGE_PAGE];gf_result_t r;size_t n;
    switch(s->etape) {
    case INVALIDER:
        buf[0]=0;if(ecrire(s,31,buf,1,now))s->etape=CONTROLER_INVALIDE;break;
    case CONTROLER_INVALIDE:
        r=s->io.read(s->io.ctx,(uint16_t)(s->cible*STOCKAGE_BANQUE+31),buf,1);
        if(r!=GF_OK || buf[0]!=0){terminer(s,r!=GF_OK?r:GF_VERIFICATION);break;}
        s->etape=CORPS;break;
    case CORPS:
        n=s->longueur-s->position;if(n>STOCKAGE_PAGE)n=STOCKAGE_PAGE;
        if(ecrire(s,s->position,s->image+s->position,n,now)) {
            s->position+=n;if(s->position==s->longueur)s->etape=HEADER;
        }break;
    case HEADER:
        if(ecrire(s,0,s->image,31,now)){s->position=0;s->etape=VERIFIER;}break;
    case VERIFIER:
        n=s->longueur-s->position;if(n>sizeof(buf))n=sizeof(buf);
        r=s->io.read(s->io.ctx,(uint16_t)(s->cible*STOCKAGE_BANQUE+s->position),buf,n);
        if(r!=GF_OK || memcmp(buf,s->image+s->position,n)) {terminer(s,r!=GF_OK?r:GF_VERIFICATION);break;}
        s->position+=n;if(s->position==s->longueur)s->etape=VALIDER;break;
    case VALIDER:
        buf[0]=MARQUEUR;if(ecrire(s,31,buf,1,now))s->etape=CONTROLER_VALIDE;break;
    case CONTROLER_VALIDE:
        r=s->io.read(s->io.ctx,(uint16_t)(s->cible*STOCKAGE_BANQUE),buf,ENTETE);
        s->image[31]=MARQUEUR;
        if(r!=GF_OK || memcmp(buf,s->image,ENTETE)){terminer(s,r!=GF_OK?r:GF_VERIFICATION);break;}
        s->actif=s->cible;s->generation=s->generation_cible;terminer(s,GF_OK);break;
    default:terminer(s,GF_ARGUMENT);break;
    }
}

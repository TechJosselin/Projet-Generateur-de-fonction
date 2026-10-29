#include "analogique.h"
#include <string.h>

static int64_t distance(int64_t a,int64_t b) {return a>b?a-b:b-a;}
static int32_t interp(int32_t a,int32_t b,unsigned ca,unsigned cb,unsigned code) {
    return (int32_t)((int64_t)a+((int64_t)b-a)*(code-ca)/(cb-ca));
}
static bool entre(int32_t x,int32_t a,int32_t b) {
    return a<b?(x>=a && x<=b):(x>=b && x<=a);
}
gf_result_t calibration_valider(const calibration_t *p) {
    if(!p)return GF_ARGUMENT;
    if(!p->approuve)return GF_NON_CALIBRE;
    if(!p->carte[0] || !memchr(p->carte,0,sizeof(p->carte)) || !p->charge_ohm ||
       p->sortie_min_mv>=p->sortie_max_mv || p->sortie_min_mv< -15000 ||
       p->sortie_max_mv>15000 || p->stabilisation_ms>100)return GF_ARGUMENT;
    for(size_t i=0;p->carte[i];i++)if((unsigned char)p->carte[i]<32 || (unsigned char)p->carte[i]>126)return GF_ARGUMENT;
    unsigned disponibles=0;
    for(unsigned f=0;f<3;f++) {
        const cal_forme_t *v=&p->formes[f];
        if(!v->nombre_gain && !v->nombre_offset)continue;
        if(v->nombre_gain<2 || v->nombre_gain>CAL_POINTS_MAX ||
           v->nombre_offset<2 || v->nombre_offset>CAL_POINTS_MAX ||
           !v->freq_min_hz || v->freq_min_hz>v->freq_max_hz ||
           v->freq_max_hz>SIGNAL_FREQ_MAX_HZ ||
           v->erreur_amplitude_mvpp>20000 || v->erreur_offset_mv>10000)return GF_ARGUMENT;
        disponibles++;
        bool croissant=v->gain[1].amplitude_mvpp>v->gain[0].amplitude_mvpp;
        for(unsigned i=0;i<v->nombre_gain;i++) {
            const cal_gain_t *g=&v->gain[i];
            if(g->amplitude_mvpp<=0 || g->amplitude_mvpp>30000 ||
               g->zero_mv< -30000 || g->zero_mv>30000)return GF_ARGUMENT;
            if(i && (g->code<=v->gain[i-1].code ||
               (croissant?g->amplitude_mvpp<=v->gain[i-1].amplitude_mvpp:
                           g->amplitude_mvpp>=v->gain[i-1].amplitude_mvpp)))return GF_ARGUMENT;
        }
        croissant=v->offset[1].delta_mv>v->offset[0].delta_mv;
        for(unsigned i=0;i<v->nombre_offset;i++) {
            const cal_offset_t *o=&v->offset[i];
            if(o->delta_mv< -30000 || o->delta_mv>30000)return GF_ARGUMENT;
            if(i && (o->code<=v->offset[i-1].code ||
               (croissant?o->delta_mv<=v->offset[i-1].delta_mv:
                           o->delta_mv>=v->offset[i-1].delta_mv)))return GF_ARGUMENT;
        }
    }
    return disponibles?GF_OK:GF_ARGUMENT;
}
gf_result_t calibration_encoder(const calibration_t *p,uint8_t b[CAL_IMAGE_SIZE]) {
    if(!p || !b)return GF_ARGUMENT;
    memset(b,0,CAL_IMAGE_SIZE);
    if(!p->approuve)return GF_OK; /* Profil absent : representation canonique nulle. */
    gf_result_t r=calibration_valider(p);if(r!=GF_OK)return r;
    memcpy(b,"CAL1",4);gf_p16(b+4,1);b[6]=1;b[7]=p->gain_avant_offset?1:0;
    memcpy(b+8,p->carte,24);gf_p32(b+32,p->charge_ohm);
    gf_p32(b+36,(uint32_t)p->sortie_min_mv);gf_p32(b+40,(uint32_t)p->sortie_max_mv);
    gf_p16(b+44,p->stabilisation_ms);
    for(unsigned f=0;f<3;f++) {
        const cal_forme_t *v=&p->formes[f];uint8_t *q=b+48+144*f;
        gf_p32(q,v->freq_min_hz);gf_p32(q+4,v->freq_max_hz);
        gf_p32(q+8,v->erreur_amplitude_mvpp);gf_p32(q+12,v->erreur_offset_mv);
        q[16]=v->nombre_gain;q[17]=v->nombre_offset;
        for(unsigned i=0;i<v->nombre_gain;i++) {
            uint8_t *g=q+24+9*i;g[0]=v->gain[i].code;
            gf_p32(g+1,(uint32_t)v->gain[i].amplitude_mvpp);gf_p32(g+5,(uint32_t)v->gain[i].zero_mv);
        }
        for(unsigned i=0;i<v->nombre_offset;i++) {
            uint8_t *o=q+96+5*i;o[0]=v->offset[i].code;gf_p32(o+1,(uint32_t)v->offset[i].delta_mv);
        }
    }
    gf_p32(b+508,gf_crc32(b,508));return GF_OK;
}
gf_result_t calibration_decoder(const uint8_t b[CAL_IMAGE_SIZE],calibration_t *p) {
    if(!b || !p)return GF_ARGUMENT;
    bool vide=true;for(unsigned i=0;i<CAL_IMAGE_SIZE;i++)if(b[i]){vide=false;break;}
    if(vide){memset(p,0,sizeof(*p));return GF_OK;}
    if(memcmp(b,"CAL1",4) || gf_u16(b+4)!=1)return GF_VERSION;
    if(gf_u32(b+508)!=gf_crc32(b,508))return GF_CRC;
    if(b[6]!=1 || b[7]>1)return GF_ARGUMENT;
    calibration_t tmp={0};tmp.approuve=true;tmp.gain_avant_offset=b[7]!=0;
    memcpy(tmp.carte,b+8,24);tmp.charge_ohm=gf_u32(b+32);
    tmp.sortie_min_mv=gf_i32(b+36);tmp.sortie_max_mv=gf_i32(b+40);tmp.stabilisation_ms=gf_u16(b+44);
    for(unsigned f=0;f<3;f++) {
        cal_forme_t *v=&tmp.formes[f];const uint8_t *q=b+48+144*f;
        v->freq_min_hz=gf_u32(q);v->freq_max_hz=gf_u32(q+4);
        v->erreur_amplitude_mvpp=gf_u32(q+8);v->erreur_offset_mv=gf_u32(q+12);
        v->nombre_gain=q[16];v->nombre_offset=q[17];
        if(v->nombre_gain>CAL_POINTS_MAX || v->nombre_offset>CAL_POINTS_MAX)return GF_ARGUMENT;
        for(unsigned i=0;i<v->nombre_gain;i++) {
            const uint8_t *g=q+24+9*i;v->gain[i]=(cal_gain_t){g[0],gf_i32(g+1),gf_i32(g+5)};
        }
        for(unsigned i=0;i<v->nombre_offset;i++) {
            const uint8_t *o=q+96+5*i;v->offset[i]=(cal_offset_t){o[0],gf_i32(o+1)};
        }
    }
    gf_result_t r=calibration_valider(&tmp);if(r==GF_OK)*p=tmp;return r;
}
static bool excursion(const calibration_t *p,int32_t a,int32_t o) {
    return 2*(int64_t)o-a>=2*(int64_t)p->sortie_min_mv &&
           2*(int64_t)o+a<=2*(int64_t)p->sortie_max_mv;
}
gf_result_t analogique_convertir(const calibration_t *p,const signal_config_t *s,analogique_commande_t *cmd) {
    if(!s || !cmd || !signaux_valide(*s))return GF_ARGUMENT;
    gf_result_t r=calibration_valider(p);if(r!=GF_OK)return r;
    const cal_forme_t *v=&p->formes[s->forme];
    if(v->nombre_gain<2)return GF_NON_SUPPORTE;
    if(s->frequence_hz<v->freq_min_hz || s->frequence_hz>v->freq_max_hz ||
       !excursion(p,s->amplitude_mvpp,s->offset_mv) ||
       !entre(s->amplitude_mvpp,v->gain[0].amplitude_mvpp,v->gain[v->nombre_gain-1].amplitude_mvpp))return GF_PLAGE;
    analogique_commande_t best={0};int32_t zero=0;int64_t err=INT64_MAX;
    for(unsigned i=1;i<v->nombre_gain;i++)for(unsigned c=v->gain[i-1].code;c<=v->gain[i].code;c++) {
        const cal_gain_t a=v->gain[i-1],b=v->gain[i];
        int32_t amp=interp(a.amplitude_mvpp,b.amplitude_mvpp,a.code,b.code,c);
        int64_t d=distance(amp,s->amplitude_mvpp);
        if(d<err){err=d;best.gain=(uint8_t)c;best.amplitude_estimee_mvpp=amp;
            zero=interp(a.zero_mv,b.zero_mv,a.code,b.code,c);}
    }
    if(err>v->erreur_amplitude_mvpp)return GF_PLAGE;
    int32_t cible=s->offset_mv-zero;
    if(!entre(cible,v->offset[0].delta_mv,v->offset[v->nombre_offset-1].delta_mv))return GF_PLAGE;
    err=INT64_MAX;
    for(unsigned i=1;i<v->nombre_offset;i++)for(unsigned c=v->offset[i-1].code;c<=v->offset[i].code;c++) {
        const cal_offset_t a=v->offset[i-1],b=v->offset[i];
        int32_t o=zero+interp(a.delta_mv,b.delta_mv,a.code,b.code,c);
        int64_t d=distance(o,s->offset_mv);
        if(d<err){err=d;best.offset=(uint8_t)c;best.offset_estime_mv=o;}
    }
    if(err>v->erreur_offset_mv || !excursion(p,best.amplitude_estimee_mvpp,best.offset_estime_mv))return GF_PLAGE;
    *cmd=best;return GF_OK;
}

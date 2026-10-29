#include "usb_protocol.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
static bool nombre(const char *s,int32_t min,int32_t max,bool signe,int32_t *out) {
    if(!s || !*s)return false;
    bool neg=false;
    if(*s=='-' || *s=='+'){if(!signe)return false;neg=*s=='-';s++;}
    if(!*s)return false;
    uint64_t v=0;
    for(;*s;s++) {
        if(*s<'0' || *s>'9')return false;
        v=v*10+(unsigned)(*s-'0');if(v>UINT32_C(2147483648))return false;
    }
    int64_t x=neg?-(int64_t)v:(int64_t)v;
    if(x<min || x>max)return false;
    *out=(int32_t)x;return true;
}
static int hex(char c){if(c>='0' && c<='9')return c-'0';if(c>='A' && c<='F')return c-'A'+10;if(c>='a' && c<='f')return c-'a'+10;return -1;}
gf_result_t usb_proto_decoder(const char *s,usb_commande_t *out) {
    if(!s || !out)return GF_ARGUMENT;
    size_t len=0;while(len<USB_TRAME_MAX && s[len])len++;
    if(len<3 || len>=USB_TRAME_MAX || s[0]!='$' || s[len-1]!='#')return GF_ARGUMENT;
    char b[USB_TRAME_MAX];memcpy(b,s+1,len-2);b[len-2]=0;
    for(size_t i=0;i<len-2;i++)if((unsigned char)b[i]<=32 || (unsigned char)b[i]>126 || b[i]=='$' || b[i]=='#')return GF_ARGUMENT;
    usb_commande_t c={0};int32_t v=0;
    if(!strcmp(b,"?")){c.action=USB_TOUT;*out=c;return GF_OK;}
    if(!strcmp(b,"STATE?")){c.action=USB_ETAT;*out=c;return GF_OK;}
    if(!strcmp(b,"DIG?")){c.action=USB_DIG_INFOS;*out=c;return GF_OK;}
    if(!strcmp(b,"CAL?")){c.action=USB_CAL_GET;*out=c;return GF_OK;}
    if(b[len-3]=='?') {
        b[len-3]=0;if(!nombre(b,1,100,false,&v))return GF_ARGUMENT;
        c.action=USB_LIRE;c.index=(size_t)(v-1);*out=c;return GF_OK;
    }
    char *t[8];size_t n=1;t[0]=b;
    for(char *p=b;*p;p++)if(*p==':') {
        if(n==8)return GF_ARGUMENT;
        *p=0;t[n++]=p+1;
    }
    for(size_t i=0;i<n;i++)if(!*t[i])return GF_ARGUMENT;
    if(n==1) {
        if(!strcmp(b,"ON"))c.action=USB_ON;
        else if(!strcmp(b,"OFF"))c.action=USB_OFF;
        else if(!strcmp(b,"SAVE"))c.action=USB_SAUVER;
        else if(!strcmp(b,"LOAD"))c.action=USB_CHARGER;
        else if(!strcmp(b,"FACTORY"))c.action=USB_USINE;
        else return GF_ARGUMENT;
    } else if(n==2 && (!strcmp(b,"SELECT") || !strcmp(b,"DELETE") || !strcmp(b,"DUP"))) {
        if(!nombre(t[1],1,100,false,&v))return GF_ARGUMENT;
        c.index=(size_t)(v-1);
        c.action=!strcmp(b,"SELECT")?USB_CHOISIR:(!strcmp(b,"DELETE")?USB_SUPPRIMER:USB_DUPLIQUER);
    } else if(!strcmp(b,"CAL")) {
        if(n==2) {
            if(!strcmp(t[1],"BEGIN"))c.action=USB_CAL_BEGIN;
            else if(!strcmp(t[1],"COMMIT"))c.action=USB_CAL_COMMIT;
            else if(!strcmp(t[1],"ABORT"))c.action=USB_CAL_ABORT;
            else if(!strcmp(t[1],"CLEAR"))c.action=USB_CAL_CLEAR;
            else return GF_ARGUMENT;
        } else if(n==4 && !strcmp(t[1],"DATA")) {
            if(!nombre(t[2],0,480,false,&v) || v%32 || strlen(t[3])!=64)return GF_ARGUMENT;
            c.action=USB_CAL_DATA;c.position=(uint16_t)v;
            for(unsigned i=0;i<32;i++) {
                int a=hex(t[3][2*i]),d=hex(t[3][2*i+1]);if(a<0 || d<0)return GF_ARGUMENT;
                c.donnees[i]=(uint8_t)(16*a+d);
            }
        } else return GF_ARGUMENT;
    } else if(!strcmp(b,"DIG") && (n==3 || n==4)) {
        if(!nombre(t[1],1,2,false,&v))return GF_ARGUMENT;
        c.index=(size_t)(v-1);c.action=USB_DIGITAL;
        if(n==3) {if(strcmp(t[2],"OFF"))return GF_ARGUMENT;c.actif=false;}
        else {
            if(!nombre(t[2],1,1000000,false,&v))return GF_ARGUMENT;
            c.frequence=(uint32_t)v;
            if(!nombre(t[3],0,1000,false,&v))return GF_ARGUMENT;
            c.rapport=(uint16_t)v;c.actif=true;
        }
    } else if(n==5) {
        if(!nombre(t[0],1,100,false,&v))return GF_ARGUMENT;
        c.action=USB_ECRIRE;c.index=(size_t)(v-1);
        if(!strcmp(t[1],"SIN"))c.signal.forme=SIGNAL_SINUS;
        else if(!strcmp(t[1],"TRI"))c.signal.forme=SIGNAL_TRIANGLE;
        else if(!strcmp(t[1],"CAR"))c.signal.forme=SIGNAL_CARRE;
        else return GF_ARGUMENT;
        if(!nombre(t[2],SIGNAL_AMPL_MIN_MVPP,SIGNAL_AMPL_MAX_MVPP,false,&c.signal.amplitude_mvpp) ||
           !nombre(t[3],SIGNAL_OFFSET_MIN_MV,SIGNAL_OFFSET_MAX_MV,true,&c.signal.offset_mv) ||
           !nombre(t[4],SIGNAL_FREQ_MIN_HZ,SIGNAL_FREQ_MAX_HZ,false,&v))return GF_ARGUMENT;
        c.signal.frequence_hz=(uint32_t)v;
    } else return GF_ARGUMENT;
    *out=c;return GF_OK;
}
bool usb_proto_formater(size_t index,const signal_config_t *s,char *dst,size_t n) {
    if(!dst || !n)return false;
    dst[0]=0;
    if(!s || index>=SIGNAL_MAX || !signaux_valide(*s))return false;
    int len=snprintf(dst,n,"$%u:%s:%"PRId32":%+"PRId32":%"PRIu32"#",
        (unsigned)index+1,signaux_type_court(s->forme),s->amplitude_mvpp,s->offset_mv,s->frequence_hz);
    return len>=0 && (size_t)len<n;
}
bool usb_proto_formater_signal(size_t i,char *dst,size_t n){return usb_proto_formater(i,signaux_lire(i),dst,n);}

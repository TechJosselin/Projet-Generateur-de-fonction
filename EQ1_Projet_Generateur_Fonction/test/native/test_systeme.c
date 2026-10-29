#include "integration.h"
#include "usb_service.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"ECHEC %s:%d: %s\n",__FILE__,__LINE__,#x);exit(1);}}while(0)
typedef struct {uint8_t b[4096];bool present,wp,busy;int64_t budget;size_t ecrits,operations;} mem_t;
static mem_t mem;
static void vierge(void){memset(&mem,0,sizeof(mem));memset(mem.b,255,sizeof(mem.b));mem.present=true;mem.budget=-1;}
static gf_result_t rd(void *ctx,uint16_t a,uint8_t *b,size_t n) {
    mem_t *m=ctx;if(!m->present)return GF_ABSENT;
    if(a>4096 || n>(size_t)(4096-a))return GF_ARGUMENT;
    memcpy(b,m->b+a,n);return GF_OK;
}
static gf_result_t wr(void *ctx,uint16_t a,const uint8_t *b,size_t n) {
    mem_t *m=ctx;if(!m->present)return GF_ABSENT;
    CHECK(n>0 && n<=32 && a/32==(a+n-1)/32 && (size_t)a+n<=4096);
    size_t k=n;
    if(m->budget>=0 && (uint64_t)m->budget<k)k=(size_t)m->budget;
    if(!m->wp)memcpy(m->b+a,b,k);
    m->ecrits+=k;m->operations++;
    if(m->budget>=0)m->budget-=(int64_t)k;
    return k==n?GF_OK:GF_IO;
}
static gf_result_t ready(void *ctx){mem_t *m=ctx;return !m->present?GF_ABSENT:(m->busy?GF_OCCUPE:GF_OK);}
static stockage_io_t io(void){return (stockage_io_t){&mem,rd,wr,ready};}
/* Profil SYNTHETIQUE pour tests uniquement. Aucun coefficient par defaut en production. */
static calibration_t fixture(void) {
    calibration_t p={0};p.approuve=true;p.gain_avant_offset=true;
    strcpy(p.carte,"TEST-SYNTHETIQUE");p.charge_ohm=1000000;p.sortie_min_mv=-12000;p.sortie_max_mv=12000;
    for(unsigned f=0;f<3;f++) {
        cal_forme_t *v=&p.formes[f];v->freq_min_hz=1;v->freq_max_hz=12000000;
        v->erreur_amplitude_mvpp=100;v->erreur_offset_mv=100;v->nombre_gain=v->nombre_offset=2;
        v->gain[0]=(cal_gain_t){0,100,(int32_t)(f*100)};v->gain[1]=(cal_gain_t){255,20000,(int32_t)(f*1000)};
        v->offset[0]=(cal_offset_t){0,-15000};v->offset[1]=(cal_offset_t){255,15000};
    }return p;
}
static void catalogue(void) {
    const signal_config_t attendu[10]={
        {SIGNAL_SINUS,1000,0,10000},{SIGNAL_SINUS,5000,0,1000000},
        {SIGNAL_SINUS,2000,3000,1000},{SIGNAL_SINUS,200,10000,500},
        {SIGNAL_TRIANGLE,20000,0,5000},{SIGNAL_TRIANGLE,4000,0,100},
        {SIGNAL_TRIANGLE,5000,-5000,200000},{SIGNAL_CARRE,10000,0,50},
        {SIGNAL_CARRE,1000,0,300},{SIGNAL_CARRE,4000,6000,1000}};
    signaux_initialiser();CHECK(signaux_nombre()==10);
    for(size_t i=0;i<10;i++)CHECK(signaux_egaux(attendu[i],*signaux_lire(i)));
    CHECK(!signaux_lire(100));CHECK(!signaux_modifier(100,attendu[0]));
    for(unsigned i=10;i<100;i++)CHECK(signaux_dupliquer(0)==(int)i);
    CHECK(signaux_dupliquer(0)<0);CHECK(!signaux_ajouter(attendu[0]));
    while(signaux_nombre()>1){CHECK(signaux_supprimer(0));}
    CHECK(!signaux_supprimer(0));
    signal_config_t s=attendu[0];signaux_variation(&s,CHAMP_FREQUENCE,1,UINT32_MAX);CHECK(s.frequence_hz==12000000);
    signaux_variation(&s,CHAMP_OFFSET,-1,UINT32_MAX);CHECK(s.offset_mv==-10000);
    char b[32];signaux_formater_tension(b,sizeof(b),INT32_MIN,true);CHECK(!strcmp(b,"-2147483.648V"));
    signaux_initialiser();puts("PASS catalogue : 10 presets, 100 entrees, bornes et formatage.");
}
static void conversion(void) {
    calibration_t p=fixture(),vide={0},recup;analogique_commande_t c={0};
    uint8_t image[512],other[512];signal_config_t s=*signaux_lire(0);
    CHECK(analogique_convertir(&vide,&s,&c)==GF_NON_CALIBRE);
    CHECK(calibration_encoder(&p,image)==GF_OK);CHECK(calibration_decoder(image,&recup)==GF_OK);
    CHECK(calibration_encoder(&recup,other)==GF_OK);CHECK(!memcmp(image,other,512));
    for(size_t i=0;i<10;i++){s=*signaux_lire(i);CHECK(analogique_convertir(&p,&s,&c)==GF_OK);}
    s=(signal_config_t){SIGNAL_SINUS,20000,10000,1000};CHECK(analogique_convertir(&p,&s,&c)==GF_PLAGE);
    s.offset_mv=0;s.frequence_hz=1000;CHECK(analogique_convertir(&p,&s,&c)==GF_OK);
    p.formes[0].freq_max_hz=100;CHECK(analogique_convertir(&p,&s,&c)==GF_PLAGE);
    p=fixture();p.formes[0].gain[1].code=0;CHECK(calibration_valider(&p)==GF_ARGUMENT);
    p=fixture();p.formes[0].nombre_gain=9;CHECK(calibration_valider(&p)==GF_ARGUMENT);
    image[50]^=1;CHECK(calibration_decoder(image,&recup)==GF_CRC);
    memset(image,0,sizeof(image));CHECK(calibration_decoder(image,&recup)==GF_OK);CHECK(!recup.approuve);
    p=fixture();p.formes[0].erreur_amplitude_mvpp=0;s.amplitude_mvpp=101;CHECK(analogique_convertir(&p,&s,&c)==GF_PLAGE);
    p=fixture();cal_gain_t a=p.formes[0].gain[0],b=p.formes[0].gain[1];
    p.formes[0].gain[0].amplitude_mvpp=b.amplitude_mvpp;p.formes[0].gain[1].amplitude_mvpp=a.amplitude_mvpp;
    CHECK(calibration_valider(&p)==GF_OK);CHECK(analogique_convertir(&p,&s,&c)==GF_OK);
    puts("PASS calibration : serialization, CRC, tables croissantes/decroissantes, excursion et quantification.");
}
static void terminer(stockage_t *s) {
    for(uint32_t t=0;stockage_occupe(s) && t<50000;t+=10)stockage_pas(s,t);
    CHECK(!stockage_occupe(s));
}
static void snapshot(stockage_donnees_t *d,size_t n) {
    memset(d,0,sizeof(*d));d->nombre=n;d->calibration=fixture();
    for(size_t i=0;i<n;i++){d->signaux[i]=*signaux_lire(i%10);d->signaux[i].frequence_hz+=(uint32_t)i;}
}
static void persistance(void) {
    stockage_t a,reboot;stockage_donnees_t d,got,new;
    vierge();stockage_initialiser(&a,io());CHECK(stockage_charger(&a,&got)==GF_VIDE);
    snapshot(&d,10);CHECK(stockage_commencer(&a,&d)==GF_OK);terminer(&a);CHECK(a.resultat==GF_OK);
    uint8_t ancien[4096];memcpy(ancien,mem.b,4096);snapshot(&new,100);new.signaux[0].frequence_hz=12345;
    size_t avant=mem.ecrits;CHECK(stockage_commencer(&a,&new)==GF_OK);terminer(&a);CHECK(a.resultat==GF_OK);
    size_t octets=mem.ecrits-avant;
    stockage_initialiser(&reboot,io());CHECK(stockage_charger(&reboot,&got)==GF_OK);CHECK(got.nombre==100);
    CHECK(got.signaux[0].frequence_hz==12345);
    /* Coupure simulee apres CHAQUE octet ecrit, dont le marqueur final. */
    for(size_t cut=0;cut<=octets;cut++) {
        memcpy(mem.b,ancien,4096);mem.budget=-1;stockage_initialiser(&a,io());
        CHECK(stockage_charger(&a,&got)==GF_OK);mem.budget=(int64_t)cut;
        CHECK(stockage_commencer(&a,&new)==GF_OK);terminer(&a);
        mem.budget=-1;stockage_initialiser(&reboot,io());CHECK(stockage_charger(&reboot,&got)==GF_OK);
        CHECK(got.nombre==10 || got.nombre==100);
        const stockage_donnees_t *ref=got.nombre==10?&d:&new;
        for(size_t i=0;i<got.nombre;i++)CHECK(signaux_egaux(got.signaux[i],ref->signaux[i]));
    }
    memcpy(mem.b,ancien,4096);mem.budget=-1;stockage_initialiser(&a,io());CHECK(stockage_charger(&a,&got)==GF_OK);
    mem.wp=true;CHECK(stockage_commencer(&a,&new)==GF_OK);terminer(&a);CHECK(a.resultat==GF_VERIFICATION);mem.wp=false;
    CHECK(stockage_commencer(&a,&new)==GF_OK);mem.busy=true;terminer(&a);CHECK(a.resultat==GF_TIMEOUT);mem.busy=false;
    memcpy(mem.b,ancien,4096);mem.b[32]^=1;stockage_initialiser(&a,io());CHECK(stockage_charger(&a,&got)==GF_CRC);
    memcpy(mem.b,ancien,4096);gf_p16(mem.b+4,77);gf_p32(mem.b+24,gf_crc32(mem.b,24));
    stockage_initialiser(&a,io());CHECK(stockage_charger(&a,&got)==GF_VERSION);
    mem.present=false;stockage_initialiser(&a,io());CHECK(stockage_charger(&a,&got)==GF_ABSENT);
    printf("PASS EEPROM : %zu points de coupure, CRC, version, WP, timeout, 100 presets.\n",octets+1);
}
static struct {unsigned appels,echec;bool toujours,actif;char sequence[64];} fake;
static gf_result_t op(char c) {
    fake.appels++;if(fake.appels<sizeof(fake.sequence))fake.sequence[fake.appels-1]=c;
    return fake.toujours || fake.appels==fake.echec?GF_IO:GF_OK;
}
static gf_result_t stop(void *p){(void)p;gf_result_t r=op('S');if(r==GF_OK)fake.actif=false;return r;}
static gf_result_t dds(void *p,const signal_config_t *s){(void)p;CHECK(signaux_valide(*s));return op('D');}
static gf_result_t gain(void *p,uint8_t c){(void)p;(void)c;return op('G');}
static gf_result_t offset(void *p,uint8_t c){(void)p;(void)c;return op('O');}
static gf_result_t on(void *p){(void)p;fake.actif=true;return op('E');}
static gf_result_t dig(void *p,unsigned c,uint32_t f,uint16_t duty,bool run){(void)p;(void)f;(void)duty;(void)run;CHECK(c<2);return op('X');}
static integration_backend_t backend(void) {
    return (integration_backend_t){.i2c=true,.dds=true,.epots=true,.eeprom=mem.present,
        .arreter=stop,.dds_configurer=dds,.gain=gain,.offset=offset,.activer=on,.digital=dig};
}
static void boot(bool cal) {
    memset(&fake,0,sizeof(fake));integration_initialiser_avec(backend(),io());
    if(cal) {calibration_t p=fixture();uint8_t b[512];CHECK(calibration_encoder(&p,b)==GF_OK);CHECK(integration_importer_calibration(b)==GF_OK);}
    CHECK(integration_choisir(0)==GF_OK);memset(&fake,0,sizeof(fake));
}
static void pompe(uint32_t debut,uint32_t fin){for(uint32_t t=debut;t<fin;t+=10)integration_periodique(t);}
static void coordination(void) {
    vierge();boot(false);CHECK(integration_activer()==GF_NON_CALIBRE);CHECK(fake.appels==0);
    for(unsigned fail=1;fail<=5;fail++) {
        vierge();boot(true);fake.echec=fail;CHECK(integration_activer()==GF_IO);
        CHECK(integration_statut()->etat==SYS_DEFAUT);CHECK(!integration_statut()->commande_courante);
        CHECK(!integration_statut()->derniere_valide);CHECK(integration_activer()==GF_CONFLIT);
        CHECK(integration_dupliquer(0,NULL)==GF_OK);
        CHECK(integration_statut()->etat==SYS_DEFAUT && integration_statut()->erreur==GF_IO);
        fake.echec=0;CHECK(integration_arreter()==GF_OK);CHECK(integration_activer()==GF_OK);
    }
    vierge();boot(true);CHECK(integration_activer()==GF_OK);CHECK(!strncmp(fake.sequence,"SDGOE",5));
    CHECK(integration_statut()->derniere.frequence_hz==10000);
    signal_config_t s=*signaux_lire(0);s.frequence_hz=20000;
    uint32_t rev=integration_statut()->revision;fake.toujours=true;
    CHECK(integration_modifier(0,s,rev)==GF_IO);CHECK(signaux_lire(0)->frequence_hz==10000);
    CHECK(integration_statut()->revision==rev);fake.toujours=false;
    CHECK(integration_modifier(0,s,rev)==GF_OK);CHECK(!fake.actif);
    CHECK(integration_statut()->derniere.frequence_hz==10000);CHECK(!integration_statut()->commande_courante);
    CHECK(integration_modifier(1,s,rev)==GF_CONFLIT);
    CHECK(integration_activer()==GF_OK);CHECK(integration_statut()->derniere.frequence_hz==20000);
    CHECK(integration_supprimer(1)==GF_OK);CHECK(integration_statut()->selection==-1);CHECK(!fake.actif);
    pompe(0,400);CHECK(integration_statut()->sauvegarde_en_cours);
    s=*signaux_lire(0);s.frequence_hz=33333;CHECK(integration_modifier(0,s,0)==GF_OK);
    pompe(400,12000);CHECK(!integration_statut()->modifie);
    size_t writes=mem.ecrits;CHECK(integration_modifier(0,s,0)==GF_OK);CHECK(integration_sauver()==GF_OK);
    pompe(12000,13000);CHECK(mem.ecrits==writes);
    integration_initialiser_avec(backend(),io());CHECK(signaux_lire(0)->frequence_hz==33333);
    CHECK(integration_statut()->selection==-1);CHECK(!fake.actif);
    mem.present=false;boot(false);s=*signaux_lire(0);s.frequence_hz=44444;
    CHECK(integration_modifier(0,s,0)==GF_OK);pompe(0,10000);CHECK(integration_statut()->modifie);
    CHECK(integration_statut()->sauvegarde==GF_ABSENT);
    CHECK(integration_commander_digital(0,1000,500,true)==GF_NON_SUPPORTE);
    vierge();integration_backend_t hw=backend();hw.digitales[0]=hw.digitales[1]=true;
    integration_initialiser_avec(hw,io());CHECK(integration_commander_digital(0,1000,500,true)==GF_OK);
    CHECK(integration_commander_digital(1,2000,300,true)==GF_OK);CHECK(integration_arreter()==GF_OK);
    CHECK(!integration_digital_statut(0)->actif && !integration_digital_statut(1)->actif);
    puts("PASS integration : pannes a chaque etape, edition active, conflits, revisions et reboot inactif.");
}
static void feed(const char *s,uint32_t t){for(;*s;s++)usb_service_octet((uint8_t)*s,t);}
static void drain(char *b,size_t capacite) {
    size_t n,total=0;const uint8_t *p;
    while((p=usb_service_tx(&n))!=NULL){CHECK(total+n<capacite);memcpy(b+total,p,n);total+=n;usb_service_tx_consomme(n,10);}
    b[total]=0;
}
static void protocole(void) {
    vierge();boot(false);usb_service_initialiser();usb_commande_t c;char b[8193];
    const char *bad[]={"","$#","$0?#","$101?#","$1:SIN:0:0:1#","$1:X:1000:0:1000#",
        "$1:SIN:1000:10001:1000#","$1:SIN:1000:0:12000001#","$1:SIN:1000:0:-1#",
        "$1:SIN:1000:0:1:2#","$1:SIN::0:1#","$1:SIN:1000:0:1#x","$1:SIN:1000:0:1 #",
        "$1:SIN:1000:--1:1#","$1:SIN:1000:0:99999999999999999999999999999#",
        "$CAL:DATA:1:00#","$DIG:0:1:500#","$DIG:1:1000:1001#","$2??#"};
    uint32_t rev=integration_statut()->revision;
    for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);i++)CHECK(usb_proto_decoder(bad[i],&c)!=GF_OK);
    CHECK(integration_statut()->revision==rev);
    CHECK(usb_proto_decoder("$100:CAR:20000:-10000:12000000#",&c)==GF_OK);CHECK(c.index==99);
    feed("bruit\r\n$1?",0);feed("#\r\n$2?#",10);drain(b,sizeof(b));
    CHECK(!strcmp(b,"$1:SIN:1000:+0:10000#$2:SIN:5000:+0:1000000#"));
    feed("$1:SIN:1200:+500:1234#",20);drain(b,sizeof(b));CHECK(strstr(b,"$OK:RAM:"));
    CHECK(signaux_lire(0)->frequence_hz==1234);CHECK(integration_statut()->modifie);
    pompe(0,8000);CHECK(!integration_statut()->modifie);
    for(unsigned i=10;i<100;i++)CHECK(signaux_ajouter(*signaux_lire(0)));
    feed("$?#",30);size_t count=0;drain(b,sizeof(b));for(char *p=b;*p;p++)if(*p=='#')count++;
    CHECK(count==100);CHECK(strstr(b,"$100:SIN:"));
    feed("$101:SIN:1000:0:1#",40);drain(b,sizeof(b));CHECK(strstr(b,"$ERR:"));
    feed("$1?",50);usb_service_periodique(1050);drain(b,sizeof(b));CHECK(strstr(b,"TIMEOUT"));
    feed("$xxxx$1?#",1100);drain(b,sizeof(b));CHECK(strstr(b,"$1:SIN:"));
    feed("$",1200);for(unsigned i=0;i<200;i++)usb_service_octet('A',1200);drain(b,sizeof(b));CHECK(strstr(b,"CAPACITE"));
    feed("$",1200);usb_service_octet(0xFF,1200);drain(b,sizeof(b));CHECK(strstr(b,"ARGUMENT"));
    feed("$1?#",2000);usb_service_periodique(4000);CHECK(usb_service_reinitialisation_transport());
    CHECK(usb_service_espace_tx()==8192);
    /* Import complet, puis lecture du meme profil par le protocole. */
    calibration_t p=fixture();uint8_t image[512];CHECK(calibration_encoder(&p,image)==GF_OK);
    feed("$CAL:BEGIN#",4100);drain(b,sizeof(b));
    for(unsigned pos=0;pos<512;pos+=32) {
        char line[96];int len=snprintf(line,sizeof(line),"$CAL:DATA:%u:",pos);
        for(unsigned i=0;i<32;i++)len+=snprintf(line+len,sizeof(line)-(size_t)len,"%02X",image[pos+i]);
        strcat(line,"#");feed(line,4100);drain(b,sizeof(b));CHECK(strstr(b,"$OK:CAL_DATA:"));
    }
    feed("$CAL:COMMIT#$CAL?#",4200);drain(b,sizeof(b));CHECK(strstr(b,"$OK:RAM:"));CHECK(strstr(b,"$CAL:END#"));
    CHECK(integration_calibration()->approuve);
    /* Petits tampons de formatage et analyse de chaines non terminees bornees. */
    struct {char buf[8];char garde;} petit={{0},'Z'};
    CHECK(!usb_proto_formater_signal(0,petit.buf,sizeof(petit.buf)));CHECK(petit.garde=='Z');CHECK(petit.buf[7]==0);
    char sans_fin[96];memset(sans_fin,'X',sizeof(sans_fin));CHECK(usb_proto_decoder(sans_fin,&c)!=GF_OK);
    uint32_t rand=1;for(unsigned k=0;k<10000;k++) {
        char f[96];for(unsigned i=0;i<95;i++){rand=1664525U*rand+1013904223U;f[i]=(char)(rand>>24);}f[95]=0;
        (void)usb_proto_decoder(f,&c);
    }
    puts("PASS USB : fragmentation, liste de 100, erreurs, backpressure, import profil, 10000 entrees aleatoires.");
}
int main(void) {
    CHECK(gf_crc32((const uint8_t *)"123456789",9)==UINT32_C(0xCBF43926));
    catalogue();conversion();persistance();coordination();protocole();
    printf("RESULTAT : %u assertions satisfaites. Tests logiciels uniquement.\n",checks);return 0;
}

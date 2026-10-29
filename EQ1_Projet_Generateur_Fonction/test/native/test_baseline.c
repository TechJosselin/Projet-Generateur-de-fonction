/* Tests de revue executes sur ordinateur, sans ESP32 ni peripheriques.
 * Origine : revue fe47f86. Le runner compile les sources courantes du depot.
 */
#include "signaux.h"
#include "usb_protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static unsigned checks;
#define CHECK(c) do { ++checks; if (!(c)) { fprintf(stderr,"ECHEC ligne %d: %s\n",__LINE__,#c); exit(1); } } while(0)
static int equal(signal_config_t a, signal_config_t b) {
    return a.forme==b.forme && a.amplitude_mvpp==b.amplitude_mvpp &&
           a.offset_mv==b.offset_mv && a.frequence_hz==b.frequence_hz;
}
int main(void) {
    char response[128], frame[128];
    signaux_initialiser();
    CHECK(signaux_nombre()==10);
    CHECK(!signaux_modifications_en_ram());
    static const signal_config_t expected[10] = {
        {SIGNAL_SINUS, 1000, 0, 10000},
        {SIGNAL_SINUS, 5000, 0, 1000000},
        {SIGNAL_SINUS, 2000, 3000, 1000},
        {SIGNAL_SINUS, 200, 10000, 500},
        {SIGNAL_TRIANGLE, 20000, 0, 5000},
        {SIGNAL_TRIANGLE, 4000, 0, 100},
        {SIGNAL_TRIANGLE, 5000, -5000, 200000},
        {SIGNAL_CARRE, 10000, 0, 50},
        {SIGNAL_CARRE, 1000, 0, 300},
        {SIGNAL_CARRE, 4000, 6000, 1000}
    };
    for (size_t i = 0; i < 10; ++i) {
        CHECK(signaux_lire(i) != NULL);
        CHECK(equal(*signaux_lire(i), expected[i]));
    }
    CHECK(signaux_lire(10) == NULL);
    CHECK(signaux_lire(SIZE_MAX) == NULL);
    for (size_t i=0;i<10;i++) {
        CHECK(signaux_valide(*signaux_lire(i)));
        CHECK(usb_proto_formater_signal(i,frame,sizeof(frame)));
        CHECK(usb_proto_traiter(frame,response,sizeof(response))==USB_PROTO_ECRITURE_RAM);
        CHECK(!signaux_modifie(i));
    }
    CHECK(usb_proto_traiter("$1?#",response,sizeof(response))==USB_PROTO_LECTURE_UN);
    CHECK(strcmp(response,"$1:SIN:1000:+0:10000#")==0);
    CHECK(usb_proto_traiter("$1:TRI:5000:-5000:200000#",response,sizeof(response))==USB_PROTO_ECRITURE_RAM);
    CHECK(strcmp(response,"OK:RAM")==0);
    CHECK(signaux_lire(0)->forme==SIGNAL_TRIANGLE);
    CHECK(signaux_lire(0)->amplitude_mvpp==5000);
    CHECK(signaux_lire(0)->offset_mv==-5000);
    CHECK(signaux_lire(0)->frequence_hz==200000);
    CHECK(signaux_modifie(0));
    CHECK(signaux_modifications_en_ram());
    puts("PASS: presets, formatage, lecture et modification RAM.");

    const char *invalid[]={
        "", "$", "$#", "1?#", "$1?", "$0?#", "$101?#", "$11?#",
        "$1:sin:1000:+0:1000#", "$1:XYZ:1000:+0:1000#",
        "$1:SIN:0:+0:1000#", "$1:SIN:20001:+0:1000#",
        "$1:SIN:1000:+10001:1000#", "$1:SIN:1000:-10001:1000#",
        "$1:SIN:1000:+0:0#", "$1:SIN:1000:+0:12000001#",
        "$1:SIN:1000:+0:1000:4#", "$1:SIN:1000:+0#",
        "$1:SIN::+0:1000#", "$1:SIN:abc:+0:1000#",
        "$1:SIN:1000:+0:1000x#", "$1?:SIN:1000:+0:1000#",
        "$99999999999999999999999999999999999999999999999999999999999999999?#"
    };
    signal_config_t before=*signaux_lire(0);
    for(size_t i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) {
        CHECK(usb_proto_traiter(invalid[i],response,sizeof(response))==USB_PROTO_ERREUR);
        CHECK(equal(*signaux_lire(0),before));
    }
    CHECK(!usb_proto_formater_signal(0,response,3));
    CHECK(usb_proto_traiter(NULL,response,sizeof(response))==USB_PROTO_ERREUR);
    CHECK(usb_proto_traiter("$1?#",NULL,0)==USB_PROTO_ERREUR);
    CHECK(usb_proto_traiter("$?#",response,sizeof(response))==USB_PROTO_LECTURE_TOUT);
    CHECK(strcmp(response,"OK:LISTE")==0);
    puts("PASS: 23 trames invalides rejetees sans changement du preset; bornes de buffers.");
    puts("CONSTAT: $?# ne genere pas la liste; il retourne USB_PROTO_LECTURE_TOUT et OK:LISTE.");

    signaux_initialiser();
    while(signaux_nombre()<100) CHECK(signaux_dupliquer(0)>=0);
    CHECK(signaux_nombre()==100);
    CHECK(signaux_dupliquer(0)==-1);
    CHECK(usb_proto_traiter("$100:CAR:1000:+0:12000000#",response,sizeof(response))==USB_PROTO_ECRITURE_RAM);
    CHECK(usb_proto_traiter("$100?#",response,sizeof(response))==USB_PROTO_LECTURE_UN);
    CHECK(strcmp(response,"$100:CAR:1000:+0:12000000#")==0);
    signal_config_t shifted=*signaux_lire(1);
    CHECK(signaux_supprimer(0));
    CHECK(equal(*signaux_lire(0),shifted));
    while(signaux_nombre()>1) CHECK(signaux_supprimer(0));
    CHECK(!signaux_supprimer(0));
    CHECK(signaux_nombre()==1);
    puts("PASS: capacite 100, numero 100, refus du 101e, suppression avec minimum un signal.");

    signaux_initialiser();
    signal_config_t set[2]={*signaux_lire(0),*signaux_lire(1)};
    set[1].frequence_hz=0;
    CHECK(!signaux_remplacer_ensemble(set,2));
    CHECK(signaux_nombre()==10);
    CHECK(equal(*signaux_lire(0),set[0]));
    set[1].frequence_hz=123;
    CHECK(signaux_remplacer_ensemble(set,2));
    CHECK(signaux_nombre()==2);
    CHECK(!signaux_modifications_en_ram());
    signal_config_t changed=*signaux_lire(0);
    signaux_variation(&changed,CHAMP_FREQUENCE,1,UINT32_MAX);
    CHECK(changed.frequence_hz==SIGNAL_FREQ_MAX_HZ);
    signaux_variation(&changed,CHAMP_FREQUENCE,-1,UINT32_MAX);
    CHECK(changed.frequence_hz==SIGNAL_FREQ_MIN_HZ);
    signaux_variation(&changed,CHAMP_AMPLITUDE,1,UINT32_MAX);
    CHECK(changed.amplitude_mvpp==SIGNAL_AMPL_MAX_MVPP);
    signaux_variation(&changed,CHAMP_OFFSET,-1,UINT32_MAX);
    CHECK(changed.offset_mv==SIGNAL_OFFSET_MIN_MV);
    puts("PASS: remplacement valide avant copie; increments extremes bornes sans debordement detecte.");

    CHECK(usb_proto_traiter("$1:SIN:20000:+10000:1000#",response,sizeof(response))==USB_PROTO_ECRITURE_RAM);
    CHECK(signaux_lire(0)->amplitude_mvpp==20000);
    CHECK(signaux_lire(0)->offset_mv==10000);
    puts("CONSTAT: le code accepte 20 Vpp avec +10 V d'offset (signal ideal de 0 a +20 V).");
    signaux_initialiser();
    CHECK(signaux_nombre()==10);
    CHECK(signaux_lire(0)->amplitude_mvpp==1000);
    CHECK(signaux_lire(0)->offset_mv==0);
    puts("CONSTAT: signaux_initialiser recharge les valeurs d'usine, sans persistance.");
    printf("RESULTAT: %u assertions satisfaites. Aucun test ESP-IDF ni test materiel.\n",checks);
    return 0;
}

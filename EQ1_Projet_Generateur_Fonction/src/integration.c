#include "integration.h"
#include <string.h>
static integration_backend_t hw;
static stockage_t memoire;
static calibration_t profil;
static integration_statut_t etat;
static digital_statut_t digitales[2];
static stockage_donnees_t instantane; /* Pas 2 ko de pile par sauvegarde. */
static uint32_t horloge,derniere_modification;
static bool sauvegarde_demandee;

const integration_statut_t *integration_statut(void){return &etat;}
const calibration_t *integration_calibration(void){return &profil;}
const digital_statut_t *integration_digital_statut(unsigned c){return c<2?&digitales[c]:NULL;}
const char *integration_nom_etat(void) {
    static const char *const noms[]={"NON_INITIALISE","ARRET_COMMANDE","PRET","ACTIF_COMMANDE","NON_CALIBRE","DEFAUT"};
    return noms[etat.etat];
}
bool integration_i2c_disponible(void){return hw.i2c;}
bool integration_ad9833_disponible(void){return hw.dds;}
bool integration_epot_disponible(void){return hw.epots;}
bool integration_memoire_disponible(void){return hw.eeprom;}
bool integration_sortie_digitale_disponible(unsigned c){return c<2 && hw.digitales[c];}
static gf_result_t resultat(gf_result_t r,const char *etape) {
    /* Le dernier defaut materiel reste visible jusqu'a un arret confirme. */
    if(etat.etat!=SYS_DEFAUT){etat.erreur=r;etat.etape=etape;}
    return r;
}
static void etat_arrete(void) {
    etat.etat=!hw.dds?SYS_NON_INITIALISE:
        (calibration_valider(&profil)!=GF_OK?SYS_NON_CALIBRE:
         (etat.selection>=0?SYS_PRET:SYS_ARRET_COMMANDE));
}
static void changer(void) {
    if(++etat.revision==0)etat.revision=1;
    etat.modifie=true;sauvegarde_demandee=true;derniere_modification=horloge;
    etat.sauvegarde=hw.eeprom?GF_OK:GF_ABSENT;
}
static gf_result_t arreter_analogique(void) {
    gf_result_t r=hw.dds && hw.arreter?hw.arreter(hw.ctx):GF_ABSENT;
    etat.erreur_arret=r;etat.commande_courante=false;
    if(r!=GF_OK){etat.erreur=r;etat.etape="ARRET_DDS";etat.etat=SYS_DEFAUT;return r;}
    etat_arrete();return resultat(GF_OK,"ARRET_DDS");
}
static gf_result_t arreter_si_necessaire(void) {
    if(etat.etat==SYS_ACTIF_COMMANDE || etat.etat==SYS_DEFAUT)return arreter_analogique();
    return GF_OK;
}
gf_result_t integration_arreter(void) {
    gf_result_t r=arreter_analogique();
    for(unsigned c=0;c<2;c++)if(hw.digitales[c]) {
        gf_result_t d=integration_commander_digital(c,0,0,false);
        if(r==GF_OK && d!=GF_OK)r=d;
    }
    if(r!=GF_OK){if(etat.etat!=SYS_DEFAUT){etat.erreur=r;etat.etape="ARRET";}etat.etat=SYS_DEFAUT;return r;}
    return resultat(GF_OK,"ARRET");
}
void integration_initialiser_avec(integration_backend_t backend,stockage_io_t io) {
    hw=backend;memset(&etat,0,sizeof(etat));memset(&profil,0,sizeof(profil));
    memset(digitales,0,sizeof(digitales));etat.selection=-1;etat.revision=1;
    horloge=0;derniere_modification=0;sauvegarde_demandee=false;
    signaux_initialiser();stockage_initialiser(&memoire,io);
    etat.chargement=hw.eeprom?stockage_charger(&memoire,&instantane):GF_ABSENT;
    etat.alerte_stockage=memoire.alerte;
    if(etat.chargement==GF_OK) {
        (void)signaux_remplacer_ensemble(instantane.signaux,instantane.nombre);
        profil=instantane.calibration;etat.revision_sauvee=etat.revision;
    } else {
        etat.modifie=true;
        /* Une memoire corrompue/version inconnue n'est jamais ecrasee au boot. */
        sauvegarde_demandee=etat.chargement==GF_VIDE;
    }
    etat.sauvegarde=hw.eeprom?GF_OK:GF_ABSENT;etat.etape="DEMARRAGE";
    etat_arrete();
    if(hw.dds)(void)arreter_analogique();
    for(unsigned c=0;c<2;c++)if(hw.digitales[c])(void)integration_commander_digital(c,0,0,false);
}
static void capturer(void) {
    instantane.nombre=signaux_nombre();instantane.calibration=profil;
    for(size_t i=0;i<instantane.nombre;i++)instantane.signaux[i]=*signaux_lire(i);
}
void integration_periodique(uint32_t now) {
    horloge=now;
    if(stockage_occupe(&memoire)) {
        stockage_pas(&memoire,now);
        if(!stockage_occupe(&memoire)) {
            etat.sauvegarde_en_cours=false;etat.sauvegarde=memoire.resultat;
            if(memoire.resultat==GF_OK) {
                etat.revision_sauvee=etat.revision_en_sauvegarde;
                if(etat.revision==etat.revision_sauvee){etat.modifie=false;signaux_marquer_sauves();}
            }
        }
        return;
    }
    if(!etat.modifie)sauvegarde_demandee=false;
    if(sauvegarde_demandee && (uint32_t)(now-derniere_modification)>=300) {
        sauvegarde_demandee=false;capturer();
        etat.sauvegarde=hw.eeprom?stockage_commencer(&memoire,&instantane):GF_ABSENT;
        etat.sauvegarde_en_cours=etat.sauvegarde==GF_OK;
        if(etat.sauvegarde_en_cours)etat.revision_en_sauvegarde=etat.revision;
        /* Apres echec, seul un nouveau changement ou SAVE redemande une tentative. */
    }
}
gf_result_t integration_choisir(size_t index) {
    if(!signaux_lire(index))return resultat(GF_INDEX,"SELECTION");
    gf_result_t r=arreter_si_necessaire();if(r!=GF_OK)return r;
    etat.selection=(int)index;etat.commande_courante=false;etat_arrete();
    return resultat(GF_OK,"SELECTION_RAM");
}
static gf_result_t defaut(gf_result_t r,const char *etape) {
    etat.erreur_arret=hw.arreter?hw.arreter(hw.ctx):GF_ABSENT;
    etat.erreur=r;etat.etape=etape;etat.etat=SYS_DEFAUT;etat.commande_courante=false;return r;
}
gf_result_t integration_activer(void) {
    if(etat.etat==SYS_DEFAUT)return resultat(GF_CONFLIT,"ARRET_REQUIS");
    const signal_config_t *s=etat.selection>=0?signaux_lire((size_t)etat.selection):NULL;
    if(!s)return resultat(GF_SELECTION,"SELECTION");
    analogique_commande_t codes;gf_result_t r=analogique_convertir(&profil,s,&codes);
    if(r!=GF_OK)return resultat(r,"CALIBRATION");
    if(!hw.dds || !hw.epots || !hw.arreter || !hw.dds_configurer || !hw.gain || !hw.offset || !hw.activer)
        return resultat(GF_ABSENT,"MATERIEL");
    if(profil.stabilisation_ms && !hw.attendre_ms)return resultat(GF_NON_SUPPORTE,"STABILISATION");
    r=arreter_analogique();if(r!=GF_OK)return r;
    r=hw.dds_configurer(hw.ctx,s);if(r!=GF_OK)return defaut(r,"DDS_CONFIG");
    if(profil.gain_avant_offset) {
        r=hw.gain(hw.ctx,codes.gain);if(r!=GF_OK)return defaut(r,"GAIN");
        r=hw.offset(hw.ctx,codes.offset);if(r!=GF_OK)return defaut(r,"OFFSET");
    } else {
        r=hw.offset(hw.ctx,codes.offset);if(r!=GF_OK)return defaut(r,"OFFSET");
        r=hw.gain(hw.ctx,codes.gain);if(r!=GF_OK)return defaut(r,"GAIN");
    }
    if(profil.stabilisation_ms)hw.attendre_ms(hw.ctx,profil.stabilisation_ms);
    r=hw.activer(hw.ctx);if(r!=GF_OK)return defaut(r,"ACTIVATION");
    etat.derniere=*s;etat.derniere_valide=true;etat.codes=codes;
    etat.commande_courante=true;etat.etat=SYS_ACTIF_COMMANDE;
    return resultat(GF_OK,"COMMANDE_COMPLETE");
}
gf_result_t integration_modifier(size_t i,signal_config_t s,uint32_t attendu) {
    if(attendu && attendu!=etat.revision)return resultat(GF_CONFLIT,"EDITION");
    if(!signaux_valide(s))return resultat(GF_ARGUMENT,"EDITION");
    if(i>signaux_nombre() || i>=SIGNAL_MAX)return resultat(GF_INDEX,"EDITION");
    const signal_config_t *v=signaux_lire(i);
    if(v && signaux_egaux(*v,s))return resultat(GF_OK,"INCHANGE");
    if((int)i==etat.selection) {
        gf_result_t r=arreter_si_necessaire();if(r!=GF_OK)return r;
        etat.commande_courante=false;
    }
    bool ok=v?signaux_modifier(i,s):signaux_ajouter(s);
    if(!ok)return resultat(GF_CAPACITE,"EDITION");
    changer();return resultat(GF_OK,"RAM");
}
gf_result_t integration_dupliquer(size_t i,int *nouveau) {
    int n=signaux_dupliquer(i);if(n<0)return resultat(GF_CAPACITE,"DUPLIQUER");
    if(nouveau)*nouveau=n;
    changer();return resultat(GF_OK,"RAM");
}
gf_result_t integration_supprimer(size_t i) {
    if(!signaux_lire(i))return resultat(GF_INDEX,"SUPPRIMER");
    if(signaux_nombre()<=1)return resultat(GF_CAPACITE,"SUPPRIMER");
    gf_result_t r=arreter_si_necessaire();if(r!=GF_OK)return r;
    (void)signaux_supprimer(i);etat.selection=-1;etat.commande_courante=false;etat_arrete();
    changer();return resultat(GF_OK,"RAM");
}
gf_result_t integration_usine(void) {
    gf_result_t r=arreter_si_necessaire();if(r!=GF_OK)return r;
    signaux_retablir_usine();etat.selection=-1;etat.commande_courante=false;etat_arrete();
    changer();return resultat(GF_OK,"RAM");
}
gf_result_t integration_sauver(void) {
    if(!hw.eeprom)return resultat(GF_ABSENT,"SAUVEGARDE");
    if(etat.modifie){sauvegarde_demandee=true;derniere_modification=horloge-300U;}
    return resultat(GF_OK,"SAUVEGARDE_DEMANDEE");
}
gf_result_t integration_charger(void) {
    if(stockage_occupe(&memoire))return resultat(GF_OCCUPE,"CHARGER");
    if(!hw.eeprom)return resultat(GF_ABSENT,"CHARGER");
    gf_result_t r=arreter_si_necessaire();if(r!=GF_OK)return r;
    r=stockage_charger(&memoire,&instantane);etat.chargement=r;etat.alerte_stockage=memoire.alerte;
    if(r!=GF_OK)return resultat(r,"CHARGER");
    (void)signaux_remplacer_ensemble(instantane.signaux,instantane.nombre);profil=instantane.calibration;
    changer();sauvegarde_demandee=false;etat.modifie=false;etat.revision_sauvee=etat.revision;
    etat.selection=-1;etat.commande_courante=false;etat_arrete();return resultat(GF_OK,"CHARGE");
}
gf_result_t integration_importer_calibration(const uint8_t b[CAL_IMAGE_SIZE]) {
    calibration_t p;gf_result_t r=calibration_decoder(b,&p);if(r!=GF_OK)return resultat(r,"PROFIL");
    r=calibration_valider(&p);if(r!=GF_OK)return resultat(r,"PROFIL");
    r=arreter_si_necessaire();if(r!=GF_OK)return r;
    profil=p;etat.commande_courante=false;etat_arrete();changer();return resultat(GF_OK,"PROFIL_RAM");
}
gf_result_t integration_effacer_calibration(void) {
    gf_result_t r=arreter_si_necessaire();if(r!=GF_OK)return r;
    memset(&profil,0,sizeof(profil));etat.commande_courante=false;etat_arrete();changer();return resultat(GF_OK,"NON_CALIBRE");
}
gf_result_t integration_commander_digital(unsigned c,uint32_t hz,uint16_t duty,bool actif) {
    if(c>=2 || duty>1000 || (actif && (!hz || hz>1000000)))return resultat(GF_ARGUMENT,"DIGITAL");
    if(!hw.digitales[c] || !hw.digital)return resultat(GF_NON_SUPPORTE,"DIGITAL");
    gf_result_t r=hw.digital(hw.ctx,c,hz,duty,actif);digitales[c].erreur=r;digitales[c].connu=r==GF_OK;
    if(r==GF_OK){digitales[c].actif=actif;digitales[c].frequence_hz=hz;digitales[c].rapport_pm=duty;}
    return resultat(r,"DIGITAL");
}

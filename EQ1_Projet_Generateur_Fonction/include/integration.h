#ifndef INTEGRATION_H
#define INTEGRATION_H
#include "stockage.h"
typedef enum {SYS_NON_INITIALISE=0,SYS_ARRET_COMMANDE,SYS_PRET,SYS_ACTIF_COMMANDE,
              SYS_NON_CALIBRE,SYS_DEFAUT} systeme_etat_t;
typedef struct {
    bool i2c,dds,epots,eeprom;
    bool digitales[2];
    void *ctx;
    gf_result_t (*arreter)(void *);
    gf_result_t (*dds_configurer)(void *,const signal_config_t *);
    gf_result_t (*gain)(void *,uint8_t);
    gf_result_t (*offset)(void *,uint8_t);
    gf_result_t (*activer)(void *);
    void (*attendre_ms)(void *,uint16_t);
    gf_result_t (*digital)(void *,unsigned,uint32_t,uint16_t,bool);
} integration_backend_t;
typedef struct {
    systeme_etat_t etat;
    int selection;
    bool derniere_valide,commande_courante;
    signal_config_t derniere;
    analogique_commande_t codes;
    uint32_t revision,revision_sauvee,revision_en_sauvegarde;
    bool modifie,sauvegarde_en_cours;
    gf_result_t erreur,erreur_arret,sauvegarde,chargement,alerte_stockage;
    const char *etape;
} integration_statut_t;
typedef struct {bool actif,connu;uint32_t frequence_hz;uint16_t rapport_pm;gf_result_t erreur;} digital_statut_t;
/* Propriete exclusive de la boucle applicative. Pas d'appel depuis une ISR. */
void integration_initialiser_avec(integration_backend_t backend,stockage_io_t stockage);
void integration_esp32_initialiser(void);
void integration_periodique(uint32_t maintenant_ms);
const integration_statut_t *integration_statut(void);
const char *integration_nom_etat(void);
const calibration_t *integration_calibration(void);
const digital_statut_t *integration_digital_statut(unsigned canal);
gf_result_t integration_choisir(size_t index);
gf_result_t integration_activer(void);
gf_result_t integration_arreter(void);
gf_result_t integration_modifier(size_t index,signal_config_t config,uint32_t revision_attendue);
gf_result_t integration_dupliquer(size_t index,int *nouvel_index);
gf_result_t integration_supprimer(size_t index);
gf_result_t integration_usine(void);
gf_result_t integration_sauver(void);
gf_result_t integration_charger(void);
gf_result_t integration_importer_calibration(const uint8_t image[CAL_IMAGE_SIZE]);
gf_result_t integration_effacer_calibration(void);
gf_result_t integration_commander_digital(unsigned canal,uint32_t hz,uint16_t rapport_pm,bool actif);
bool integration_i2c_disponible(void);
bool integration_ad9833_disponible(void);
bool integration_epot_disponible(void);
bool integration_memoire_disponible(void);
bool integration_sortie_digitale_disponible(unsigned canal);
#endif

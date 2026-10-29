#ifndef STOCKAGE_H
#define STOCKAGE_H
#include "analogique.h"
#define STOCKAGE_TAILLE 4096U
#define STOCKAGE_BANQUE 2048U
#define STOCKAGE_PAGE 32U
#define STOCKAGE_VERSION 1U
/* IO sans boucle d'attente : page_write lance le cycle, ready est interroge
 * par stockage_pas. Un seul proprietaire (boucle applicative). */
typedef struct {
    void *ctx;
    gf_result_t (*read)(void *,uint16_t,uint8_t *,size_t);
    gf_result_t (*page_write)(void *,uint16_t,const uint8_t *,size_t);
    gf_result_t (*ready)(void *);
} stockage_io_t;
typedef struct {
    size_t nombre;
    signal_config_t signaux[SIGNAL_MAX];
    calibration_t calibration;
} stockage_donnees_t;
typedef struct {
    stockage_io_t io;
    int actif,cible;
    uint32_t generation,generation_cible,attente_depuis;
    size_t longueur,position;
    unsigned etape;
    bool attend;
    gf_result_t resultat,alerte;
    uint8_t image[STOCKAGE_BANQUE];
} stockage_t;
void stockage_initialiser(stockage_t *s,stockage_io_t io);
gf_result_t stockage_charger(stockage_t *s,stockage_donnees_t *donnees);
gf_result_t stockage_commencer(stockage_t *s,const stockage_donnees_t *donnees);
void stockage_pas(stockage_t *s,uint32_t maintenant_ms);
bool stockage_occupe(const stockage_t *s);
#endif

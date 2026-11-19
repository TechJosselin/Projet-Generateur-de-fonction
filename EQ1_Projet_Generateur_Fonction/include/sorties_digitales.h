/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : SORTIES_DIGITALES.H
 * DESCRIPTIF : Pilotage de deux sorties numeriques avec LEDC.
 *==================================================================================================*/
#ifndef SORTIES_DIGITALES_H
#define SORTIES_DIGITALES_H
#include "gf.h"

/*============================== VARIABLES ET CONFIGURATION =======================================*/
void digitales_initialiser(void);
bool digitales_disponible(unsigned canal);
gf_result_t digitales_commander(unsigned canal, uint32_t hz, uint16_t rapport_pm, bool actif);
#endif

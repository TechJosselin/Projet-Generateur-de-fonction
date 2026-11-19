/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : SORTIES_DIGITALES.H
 * DESCRIPTIF : Pilotage de deux sorties numeriques avec LEDC.
 *==================================================================================================*/
#ifndef SORTIES_DIGITALES_H
#define SORTIES_DIGITALES_H
#include "gf.h"

/*============================== VARIABLES ET CONFIGURATION =======================================*/
/* Contraintes partagees entre commandes serie et sorties LEDC. */
#define DIGITAL_NOMBRE_CANAUX             2U
#define DIGITAL_FREQUENCE_MIN_HZ          10U
#define DIGITAL_FREQUENCE_MAX_HZ          1000000U
#define DIGITAL_RAPPORT_MAX_PM            1000U
void digitales_initialiser(void);
bool digitales_disponible(unsigned canal);
gf_result_t digitales_commander(unsigned canal, uint32_t hz, uint16_t rapport_pm, bool actif);
#endif

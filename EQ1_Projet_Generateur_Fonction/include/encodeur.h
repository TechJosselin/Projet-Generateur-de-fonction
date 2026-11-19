/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : ENCODEUR.H
 * DESCRIPTIF : Interruption et acquisition de l'encodeur rotatif.
 *==================================================================================================*/
#ifndef ENCODEUR_H
#define ENCODEUR_H
#include "entree.h"

/*============================== VARIABLES ET CONFIGURATION =======================================*/
bool encodeur_initialiser(void);
evenement_ui_t encodeur_lire_evenement(void);
uint32_t encodeur_pertes(void);
#endif

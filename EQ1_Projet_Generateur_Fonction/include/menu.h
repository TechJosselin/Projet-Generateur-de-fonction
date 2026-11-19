/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : MENU.H
 * DESCRIPTIF : Navigation OLED a trois lignes et choix des signaux.
 *==================================================================================================*/
#ifndef MENU_H
#define MENU_H
#include "entree.h"

/*============================== VARIABLES ET CONFIGURATION =======================================*/
void menu_initialiser(void);
void menu_traiter(evenement_ui_t evenement);
void menu_periodique(uint32_t maintenant_ms);
#endif

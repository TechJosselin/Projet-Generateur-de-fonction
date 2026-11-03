#ifndef MENU_H
#define MENU_H
#include "entree.h"
void menu_initialiser(void);
void menu_traiter(evenement_ui_t evenement);
void menu_periodique(uint32_t maintenant_ms);
#endif

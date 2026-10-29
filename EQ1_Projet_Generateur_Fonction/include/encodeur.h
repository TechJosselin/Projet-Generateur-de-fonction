#ifndef ENCODEUR_H
#define ENCODEUR_H
#include "entree.h"
bool encodeur_initialiser(void);
evenement_ui_t encodeur_lire_evenement(void);
uint32_t encodeur_pertes(void);
#endif

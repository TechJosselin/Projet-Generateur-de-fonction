#ifndef ENTREE_H
#define ENTREE_H
#include <stdbool.h>
#include <stdint.h>
typedef enum {UI_AUCUN=0,UI_ROTATION_PLUS,UI_ROTATION_MOINS,UI_CLIC,UI_APPUI_LONG} evenement_ui_t;
typedef struct {uint8_t precedent,detente;int somme;} quadrature_t;
void quadrature_initialiser(quadrature_t *q,uint8_t ab);
int quadrature_traiter(quadrature_t *q,uint8_t ab);
typedef struct {bool brut,stable,long_emis;uint32_t rebond,appui;} bouton_t;
void bouton_initialiser(bouton_t *b,bool appuye,uint32_t now);
evenement_ui_t bouton_traiter(bouton_t *b,bool appuye,uint32_t now);
#endif

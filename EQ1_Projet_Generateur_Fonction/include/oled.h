#ifndef OLED_H
#define OLED_H
#include <stdbool.h>
#include <stdint.h>
bool oled_initialiser(void);
bool oled_est_present(void);
uint32_t oled_erreurs(void);
void oled_effacer(void);
void oled_texte(int ligne,int colonne,const char *texte);
void oled_ligne(int ligne,const char *texte,bool surligner);
/* Rendu repris de feature/menu-oled-3-lignes : pas de 9 px, hauteur 14 px.
 * Les listes limitent leur fenetre a 13 caracteres. Dessin sans transfert I2C. */
#define OLED_GRAND_PAS_X 9
#define OLED_GRAND_HAUTEUR 14
#define OLED_GRAND_NB_CARACTERES 13
void oled_texte_grand(int x,int y,const char *texte);
void oled_inverser_zone(int x,int y,int largeur,int hauteur);
void oled_pixel(int x,int y);
void oled_segment(int x0,int y0,int x1,int y1);
void oled_periodique(uint32_t maintenant_ms);
#endif

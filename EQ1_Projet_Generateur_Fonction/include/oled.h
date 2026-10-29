#ifndef OLED_H
#define OLED_H

#include <stdbool.h>
#include <stdint.h>

#define OLED_COLONNES_TEXTE 21
#define OLED_LIGNES_TEXTE 8

bool oled_initialiser(void);
bool oled_est_present(void);
uint8_t oled_adresse(void);
void oled_effacer(void);
void oled_ligne(int numero, const char *texte, bool surligner);
void oled_texte(int numero, int colonne, const char *texte);
/* Police menu : ~9 x 14 pixels (1.5x horizontal, 2x vertical). */
#define OLED_GRAND_PAS_X 9
#define OLED_GRAND_NB_CARACTERES 13
void oled_texte_grand(int x, int y, const char *texte);
void oled_inverser_zone(int x, int y, int largeur, int hauteur);
void oled_pixel(int x, int y);
void oled_segment(int x0, int y0, int x1, int y1);
void oled_actualiser(void);

#endif /* OLED_H */

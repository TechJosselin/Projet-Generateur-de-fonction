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
void oled_pixel(int x, int y);
void oled_segment(int x0, int y0, int x1, int y1);
void oled_actualiser(void);

#endif /* OLED_H */

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
void oled_pixel(int x,int y);
void oled_segment(int x0,int y0,int x1,int y1);
void oled_periodique(uint32_t maintenant_ms);
#endif

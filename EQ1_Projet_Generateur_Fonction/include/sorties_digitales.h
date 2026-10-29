#ifndef SORTIES_DIGITALES_H
#define SORTIES_DIGITALES_H
#include "gf.h"
void digitales_initialiser(void);
bool digitales_disponible(unsigned canal);
gf_result_t digitales_commander(unsigned canal,uint32_t hz,uint16_t rapport_pm,bool actif);
#endif

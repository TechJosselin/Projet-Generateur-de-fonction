#ifndef SIGNAUX_H
#define SIGNAUX_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 10 presets initiaux du cahier des charges, capacite cible 100. */
#define SIGNAL_MAX 100U
#define SIGNAL_INIT_COUNT 10U
#define SIGNAL_FREQ_MIN_HZ 1U
#define SIGNAL_FREQ_MAX_HZ 12000000U
#define SIGNAL_AMPL_MIN_MVPP 100
#define SIGNAL_AMPL_MAX_MVPP 20000
#define SIGNAL_OFFSET_MIN_MV (-10000)
#define SIGNAL_OFFSET_MAX_MV 10000

typedef enum { SIGNAL_SINUS=0, SIGNAL_TRIANGLE=1, SIGNAL_CARRE=2 } signal_forme_t;
typedef struct {
    signal_forme_t forme;
    int32_t amplitude_mvpp;
    int32_t offset_mv;
    uint32_t frequence_hz;
} signal_config_t;
typedef enum {
    CHAMP_FORME=0, CHAMP_AMPLITUDE=1, CHAMP_OFFSET=2, CHAMP_FREQUENCE=3
} signal_champ_t;

void signaux_initialiser(void);
void signaux_retablir_usine(void);
bool signaux_remplacer_ensemble(const signal_config_t *donnees, size_t nombre);
size_t signaux_nombre(void);
const signal_config_t *signaux_lire(size_t index);
bool signaux_valide(signal_config_t s);
bool signaux_modifier(size_t index, signal_config_t s);
int signaux_dupliquer(size_t index);
bool signaux_supprimer(size_t index);
bool signaux_modifie(size_t index);
size_t signaux_nombre_modifies(void);
bool signaux_modifications_en_ram(void);
void signaux_variation(signal_config_t *s, signal_champ_t champ, int direction, uint32_t pas);
const char *signaux_type_court(signal_forme_t type);
const char *signaux_type_long(signal_forme_t type);
void signaux_formater_tension(char *dst,size_t taille,int32_t mv,bool signe);
void signaux_formater_frequence(char *dst,size_t taille,uint32_t hz);
#endif

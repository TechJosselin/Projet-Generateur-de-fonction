#ifndef ANALOGIQUE_H
#define ANALOGIQUE_H
#include "gf.h"
#include "signaux.h"
#define CAL_POINTS_MAX 8U
#define CAL_IMAGE_SIZE 512U
/* Tables mesurees, codes strictement croissants, tensions monotones.
 * Le niveau moyen du DDS apres gain est interpole avec l'amplitude.
 * Le modele suppose que l'offset s'ajoute independamment : a verifier au banc. */
typedef struct {uint8_t code;int32_t amplitude_mvpp, zero_mv;} cal_gain_t;
typedef struct {uint8_t code;int32_t delta_mv;} cal_offset_t;
typedef struct {
    uint32_t freq_min_hz, freq_max_hz;
    uint32_t erreur_amplitude_mvpp, erreur_offset_mv;
    uint8_t nombre_gain, nombre_offset;
    cal_gain_t gain[CAL_POINTS_MAX];
    cal_offset_t offset[CAL_POINTS_MAX];
} cal_forme_t;
typedef struct {
    bool approuve;
    bool gain_avant_offset;
    char carte[24];
    uint32_t charge_ohm;
    int32_t sortie_min_mv, sortie_max_mv;
    uint16_t stabilisation_ms;
    cal_forme_t formes[3];
} calibration_t;
typedef struct {uint8_t gain,offset;int32_t amplitude_estimee_mvpp,offset_estime_mv;} analogique_commande_t;
gf_result_t calibration_valider(const calibration_t *p);
gf_result_t calibration_encoder(const calibration_t *p,uint8_t image[CAL_IMAGE_SIZE]);
gf_result_t calibration_decoder(const uint8_t image[CAL_IMAGE_SIZE],calibration_t *p);
gf_result_t analogique_convertir(const calibration_t *p, const signal_config_t *s,
                                analogique_commande_t *commande);
#endif

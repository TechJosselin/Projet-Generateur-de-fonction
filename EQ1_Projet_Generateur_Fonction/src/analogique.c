/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : ANALOGIQUE.C
 * DESCRIPTIF : Profils de calibration et calcul des codes de gain et d'offset.
 *==================================================================================================*/
#include "analogique.h"
#include <string.h>

/*============================== VARIABLES ET CONFIGURATION =======================================*/
/* Contraintes des profils et format binaire CAL1 (512 octets). */
#define CAL_FORMES_NOMBRE                   3U
#define CAL_POINTS_MIN                     2U
#define CAL_RAIL_MAX_MV                   15000
#define CAL_STABILISATION_MAX_MS           100U
#define CAL_ERREUR_AMPL_MAX_MVPP         20000U
#define CAL_ERREUR_OFFSET_MAX_MV         10000U
#define CAL_TENSION_TABLE_MAX_MV         30000
#define CAL_ASCII_MIN                       32U
#define CAL_ASCII_MAX                      126U
#define CAL_SIGNATURE                     "CAL1"
#define CAL_SIGNATURE_OCTETS                4U
#define CAL_VERSION                         1U
#define CAL_POSITION_VERSION                4U
#define CAL_POSITION_APPROUVE               6U
#define CAL_POSITION_ORDRE                  7U
#define CAL_POSITION_IDENTIFIANT            8U
#define CAL_IDENTIFIANT_OCTETS              24U
#define CAL_POSITION_CHARGE                32U
#define CAL_POSITION_SORTIE_MIN            36U
#define CAL_POSITION_SORTIE_MAX            40U
#define CAL_POSITION_STABILISATION         44U
#define CAL_POSITION_FORMES                48U
#define CAL_FORME_TAILLE_OCTETS           144U
#define CAL_FORME_FREQ_MAX_POSITION         4U
#define CAL_FORME_ERREUR_AMPL_POSITION      8U
#define CAL_FORME_ERREUR_OFFSET_POSITION   12U
#define CAL_FORME_NB_GAIN_POSITION         16U
#define CAL_FORME_NB_OFFSET_POSITION       17U
#define CAL_FORME_GAIN_POSITION            24U
#define CAL_FORME_GAIN_POINT_OCTETS         9U
#define CAL_FORME_OFFSET_POSITION          96U
#define CAL_FORME_OFFSET_POINT_OCTETS       5U
#define CAL_POINT_AMPLITUDE_POSITION        1U
#define CAL_POINT_ZERO_POSITION             5U
#define CAL_CRC_POSITION                   (CAL_IMAGE_SIZE - 4U)
#define CAL_EPOT_CODE_MAX                 255U
/* Estimation NON MESUREE tant que le montage n'est pas calibre au banc. */
#define CAL_NOMINAL_DDS_AMPL_MVPP          600LL
#define CAL_NOMINAL_ETAGE_GAIN             50LL
#define CAL_NOMINAL_RAIL_MV             15000LL
/*==================================== FONCTIONS ================================================*/
static int64_t distance(int64_t premiere_valeur, int64_t seconde_valeur)
{
    if (premiere_valeur > seconde_valeur)
    {
        return premiere_valeur - seconde_valeur;
    }
    return seconde_valeur - premiere_valeur;
}

static int32_t interp(int32_t premiere_valeur, int32_t seconde_valeur, unsigned code_depart, unsigned code_arrivee, unsigned code_courant)
{
    return(int32_t)((int64_t) premiere_valeur +((int64_t) seconde_valeur - premiere_valeur) *(code_courant - code_depart) /(code_arrivee - code_depart));
}

static bool entre(int32_t valeur, int32_t borne_a, int32_t borne_b)
{
    if (borne_a < borne_b)
    {
        return valeur >= borne_a && valeur <= borne_b;
    }
    return valeur >= borne_b && valeur <= borne_a;
}

/*================================ INTERFACE PUBLIQUE ============================================*/
/*--------------------------------------------------------------------------------------------------
 * Fonction   : calibration_valider()
 * Descriptif : Verifie la coherence des points d'etalonnage.
 *--------------------------------------------------------------------------------------------------*/
gf_result_t calibration_valider(const calibration_t *profil)
{
    if (!profil)
    {
        return GF_ARGUMENT;
    }
    if (!profil->approuve)
    {
        return GF_NON_CALIBRE;
    }
    if (!profil->carte[0] || !memchr(profil->carte, 0, sizeof(profil->carte)) || !profil->charge_ohm || profil->sortie_min_mv >= profil->sortie_max_mv
        || profil->sortie_min_mv < -CAL_RAIL_MAX_MV || profil->sortie_max_mv > CAL_RAIL_MAX_MV || profil->stabilisation_ms > CAL_STABILISATION_MAX_MS)
    {
        return GF_ARGUMENT;
    }
    for (size_t index_point = 0; profil->carte[index_point]; index_point++)
    {
        if ((unsigned char) profil->carte[index_point] < CAL_ASCII_MIN ||(unsigned char) profil->carte[index_point] > CAL_ASCII_MAX)
        {
            return GF_ARGUMENT;
        }
    }
    unsigned formes_disponibles = 0;
    for (unsigned index_forme = 0; index_forme < CAL_FORMES_NOMBRE; index_forme++)
    {
        const cal_forme_t *calibration_forme =&profil->formes[index_forme];
        if (!calibration_forme->nombre_gain && !calibration_forme->nombre_offset)
        {
            continue;
        }
        if (calibration_forme->nombre_gain < CAL_POINTS_MIN || calibration_forme->nombre_gain > CAL_POINTS_MAX || calibration_forme->nombre_offset < CAL_POINTS_MIN || calibration_forme->nombre_offset > CAL_POINTS_MAX
            || !calibration_forme->freq_min_hz || calibration_forme->freq_min_hz > calibration_forme->freq_max_hz || calibration_forme->freq_max_hz > SIGNAL_FREQ_MAX_HZ || calibration_forme->erreur_amplitude_mvpp > CAL_ERREUR_AMPL_MAX_MVPP
            || calibration_forme->erreur_offset_mv > CAL_ERREUR_OFFSET_MAX_MV)
        {
            return GF_ARGUMENT;
        }
        formes_disponibles++;
        bool croissant = calibration_forme->gain[1].amplitude_mvpp > calibration_forme->gain[0].amplitude_mvpp;
        for (unsigned index_point = 0; index_point < calibration_forme->nombre_gain; index_point++)
        {
            const cal_gain_t *point_gain =&calibration_forme->gain[index_point];
            if (point_gain->amplitude_mvpp <= 0 || point_gain->amplitude_mvpp > CAL_TENSION_TABLE_MAX_MV || point_gain->zero_mv < -CAL_TENSION_TABLE_MAX_MV || point_gain->zero_mv > CAL_TENSION_TABLE_MAX_MV)
            {
                return GF_ARGUMENT;
            }
            if (index_point &&(point_gain->code <= calibration_forme->gain[index_point - 1].code ||((croissant && point_gain->amplitude_mvpp <= calibration_forme->gain[index_point - 1].amplitude_mvpp) ||(!croissant
                && point_gain->amplitude_mvpp >= calibration_forme->gain[index_point - 1].amplitude_mvpp))))
            {
                return GF_ARGUMENT;
            }
        }
        croissant = calibration_forme->offset[1].delta_mv > calibration_forme->offset[0].delta_mv;
        for (unsigned index_point = 0; index_point < calibration_forme->nombre_offset; index_point++)
        {
            const cal_offset_t *point_offset =&calibration_forme->offset[index_point];
            if (point_offset->delta_mv < -CAL_TENSION_TABLE_MAX_MV || point_offset->delta_mv > CAL_TENSION_TABLE_MAX_MV)
            {
                return GF_ARGUMENT;
            }
            if (index_point &&(point_offset->code <= calibration_forme->offset[index_point - 1].code ||((croissant && point_offset->delta_mv <= calibration_forme->offset[index_point - 1].delta_mv) ||(!croissant && point_offset->delta_mv >= calibration_forme->offset[index_point - 1].delta_mv))))
            {
                return GF_ARGUMENT;
            }
        }
    }
    if (formes_disponibles)
    {
        return GF_OK;
    }
    return GF_ARGUMENT;
}

gf_result_t calibration_encoder(const calibration_t *profil, uint8_t image_binaire[CAL_IMAGE_SIZE])
{
    if (!profil || !image_binaire)
    {
        return GF_ARGUMENT;
    }
    memset(image_binaire, 0, CAL_IMAGE_SIZE);
    if (!profil->approuve)
    {
        return GF_OK;
    }
    /* Profil absent : representation canonique nulle. */
    gf_result_t resultat = calibration_valider(profil);
    if (resultat != GF_OK)
    {
        return resultat;
    }
    memcpy(image_binaire, CAL_SIGNATURE, CAL_SIGNATURE_OCTETS);
    gf_p16(image_binaire + CAL_POSITION_VERSION, CAL_VERSION);
    image_binaire[CAL_POSITION_APPROUVE] = 1;
    image_binaire[CAL_POSITION_ORDRE] = 0;
    if (profil->gain_avant_offset)
    {
        image_binaire[CAL_POSITION_ORDRE] = 1;
    }
    memcpy(image_binaire + CAL_POSITION_IDENTIFIANT, profil->carte, CAL_IDENTIFIANT_OCTETS);
    gf_p32(image_binaire + CAL_POSITION_CHARGE, profil->charge_ohm);
    gf_p32(image_binaire + CAL_POSITION_SORTIE_MIN, (uint32_t) profil->sortie_min_mv);
    gf_p32(image_binaire + CAL_POSITION_VERSION0, (uint32_t) profil->sortie_max_mv);
    gf_p16(image_binaire + CAL_POSITION_VERSION4, profil->stabilisation_ms);
    for (unsigned index_forme = 0; index_forme < CAL_FORMES_NOMBRE; index_forme++)
    {
        const cal_forme_t *calibration_forme =&profil->formes[index_forme];
        uint8_t *bloc_forme = image_binaire + CAL_POSITION_FORMES + CAL_FORME_TAILLE_OCTETS * index_forme;
        gf_p32(bloc_forme, calibration_forme->freq_min_hz);
        gf_p32(bloc_forme + CAL_FORME_FREQ_MAX_POSITION, calibration_forme->freq_max_hz);
        gf_p32(bloc_forme + CAL_FORME_ERREUR_AMPL_POSITION, calibration_forme->erreur_amplitude_mvpp);
        gf_p32(bloc_forme + CAL_FORME_ERREUR_OFFSET_POSITION, calibration_forme->erreur_offset_mv);
        bloc_forme[CAL_FORME_NB_GAIN_POSITION] = calibration_forme->nombre_gain;
        bloc_forme[CAL_FORME_NB_OFFSET_POSITION] = calibration_forme->nombre_offset;
        for (unsigned index_point = 0; index_point < calibration_forme->nombre_gain; index_point++)
        {
            uint8_t *bloc_gain = bloc_forme + CAL_FORME_GAIN_POSITION + CAL_FORME_GAIN_POINT_OCTETS * index_point;
            bloc_gain[0] = calibration_forme->gain[index_point].code;
            gf_p32(bloc_gain + CAL_POINT_AMPLITUDE_POSITION, (uint32_t) calibration_forme->gain[index_point].amplitude_mvpp);
            gf_p32(bloc_gain + CAL_POINT_ZERO_POSITION, (uint32_t) calibration_forme->gain[index_point].zero_mv);
        }
        for (unsigned index_point = 0; index_point < calibration_forme->nombre_offset; index_point++)
        {
            uint8_t *bloc_offset = bloc_forme + CAL_FORME_OFFSET_POSITION + CAL_FORME_OFFSET_POINT_OCTETS * index_point;
            bloc_offset[0] = calibration_forme->offset[index_point].code;
            gf_p32(bloc_offset + CAL_POINT_AMPLITUDE_POSITION, (uint32_t) calibration_forme->offset[index_point].delta_mv);
        }
    }
    gf_p32(image_binaire + CAL_CRC_POSITION, gf_crc32(image_binaire, CAL_CRC_POSITION));
    return GF_OK;
}

gf_result_t calibration_decoder(const uint8_t image_binaire[CAL_IMAGE_SIZE], calibration_t *profil)
{
    if (!image_binaire || !profil)
    {
        return GF_ARGUMENT;
    }
    bool image_vide = true;
    for (unsigned index_point = 0; index_point < CAL_IMAGE_SIZE; index_point++)
    {
        if (image_binaire[index_point])
        {
            image_vide = false;
            break;
        }
    }
    if (image_vide)
    {
        memset(profil, 0, sizeof(*profil));
        return GF_OK;
    }
    if (memcmp(image_binaire, CAL_SIGNATURE, CAL_SIGNATURE_OCTETS) || gf_u16(image_binaire + CAL_POSITION_VERSION) != CAL_VERSION)
    {
        return GF_VERSION;
    }
    if (gf_u32(image_binaire + CAL_CRC_POSITION) != gf_crc32(image_binaire, CAL_CRC_POSITION))
    {
        return GF_CRC;
    }
    if (image_binaire[CAL_POSITION_APPROUVE] != 1 || image_binaire[CAL_POSITION_ORDRE] > 1)
    {
        return GF_ARGUMENT;
    }
    calibration_t profil_decode = { 0 };
    profil_decode.approuve = true;
    profil_decode.gain_avant_offset = image_binaire[CAL_POSITION_ORDRE] != 0;
    memcpy(profil_decode.carte, image_binaire + CAL_POSITION_IDENTIFIANT, CAL_IDENTIFIANT_OCTETS);
    profil_decode.charge_ohm = gf_u32(image_binaire + CAL_POSITION_CHARGE);
    profil_decode.sortie_min_mv = gf_i32(image_binaire + CAL_POSITION_SORTIE_MIN);
    profil_decode.sortie_max_mv = gf_i32(image_binaire + CAL_POSITION_VERSION0);
    profil_decode.stabilisation_ms = gf_u16(image_binaire + CAL_POSITION_VERSION4);
    for (unsigned index_forme = 0; index_forme < CAL_FORMES_NOMBRE; index_forme++)
    {
        cal_forme_t *calibration_forme =&profil_decode.formes[index_forme];
        const uint8_t *bloc_forme = image_binaire + CAL_POSITION_FORMES + CAL_FORME_TAILLE_OCTETS * index_forme;
        calibration_forme->freq_min_hz = gf_u32(bloc_forme);
        calibration_forme->freq_max_hz = gf_u32(bloc_forme + CAL_FORME_FREQ_MAX_POSITION);
        calibration_forme->erreur_amplitude_mvpp = gf_u32(bloc_forme + CAL_FORME_ERREUR_AMPL_POSITION);
        calibration_forme->erreur_offset_mv = gf_u32(bloc_forme + CAL_FORME_ERREUR_OFFSET_POSITION);
        calibration_forme->nombre_gain = bloc_forme[CAL_FORME_NB_GAIN_POSITION];
        calibration_forme->nombre_offset = bloc_forme[CAL_FORME_NB_OFFSET_POSITION];
        if (calibration_forme->nombre_gain > CAL_POINTS_MAX || calibration_forme->nombre_offset > CAL_POINTS_MAX)
        {
            return GF_ARGUMENT;
        }
        for (unsigned index_point = 0; index_point < calibration_forme->nombre_gain; index_point++)
        {
            const uint8_t *bloc_gain = bloc_forme + CAL_FORME_GAIN_POSITION + CAL_FORME_GAIN_POINT_OCTETS * index_point;
            calibration_forme->gain[index_point] =(cal_gain_t)
            {
                bloc_gain[0], gf_i32(bloc_gain + CAL_POINT_AMPLITUDE_POSITION), gf_i32(bloc_gain + CAL_POINT_ZERO_POSITION)
            };
        }
        for (unsigned index_point = 0; index_point < calibration_forme->nombre_offset; index_point++)
        {
            const uint8_t *bloc_offset = bloc_forme + CAL_FORME_OFFSET_POSITION + CAL_FORME_OFFSET_POINT_OCTETS * index_point;
            calibration_forme->offset[index_point] =(cal_offset_t)
            {
                bloc_offset[0], gf_i32(bloc_offset + CAL_POINT_AMPLITUDE_POSITION)
            };
        }
    }
    gf_result_t resultat = calibration_valider(&profil_decode);
    if (resultat == GF_OK)
    {
        *profil = profil_decode;
    }
    return resultat;
}

static bool excursion(const calibration_t *profil, int32_t amplitude_mvpp, int32_t offset_mv)
{
    return 2 *(int64_t) offset_mv - amplitude_mvpp >= 2 *(int64_t) profil->sortie_min_mv && 2 *(int64_t) offset_mv + amplitude_mvpp <= 2 *(int64_t) profil->sortie_max_mv;
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : analogique_convertir()
 * Descriptif : Interpole les mesures pour trouver les codes EPOT.
 *--------------------------------------------------------------------------------------------------*/
gf_result_t analogique_convertir(const calibration_t *profil, const signal_config_t *signal, analogique_commande_t *commande)
{
    if (!signal || !commande || !signaux_valide(*signal))
    {
        return GF_ARGUMENT;
    }
    gf_result_t resultat = calibration_valider(profil);
    if (resultat != GF_OK)
    {
        return resultat;
    }
    const cal_forme_t *calibration_forme =&profil->formes[signal->forme];
    if (calibration_forme->nombre_gain < CAL_POINTS_MIN)
    {
        return GF_NON_SUPPORTE;
    }
    if (signal->frequence_hz < calibration_forme->freq_min_hz || signal->frequence_hz > calibration_forme->freq_max_hz || !excursion(profil, signal->amplitude_mvpp, signal->offset_mv)
        || !entre(signal->amplitude_mvpp, calibration_forme->gain[0].amplitude_mvpp, calibration_forme->gain[calibration_forme->nombre_gain - 1].amplitude_mvpp))
    {
        return GF_PLAGE;
    }
    analogique_commande_t meilleur_reglage = { 0 };
    int32_t niveau_zero_mv = 0;
    int64_t erreur_minimale = INT64_MAX;
    for (unsigned index_segment = 1; index_segment < calibration_forme->nombre_gain; index_segment++)
    {
        for (unsigned code_potentiometre = calibration_forme->gain[index_segment - 1].code; code_potentiometre <= calibration_forme->gain[index_segment].code; code_potentiometre++)
        {
            const cal_gain_t point_debut = calibration_forme->gain[index_segment - 1], point_fin = calibration_forme->gain[index_segment];
            int32_t amplitude_estimee_mvpp = interp(point_debut.amplitude_mvpp, point_fin.amplitude_mvpp, point_debut.code, point_fin.code, code_potentiometre);
            int64_t ecart_absolu = distance(amplitude_estimee_mvpp, signal->amplitude_mvpp);
            if (ecart_absolu < erreur_minimale)
            {
                erreur_minimale = ecart_absolu;
                meilleur_reglage.gain =(uint8_t) code_potentiometre;
                meilleur_reglage.amplitude_estimee_mvpp = amplitude_estimee_mvpp;
                niveau_zero_mv = interp(point_debut.zero_mv, point_fin.zero_mv, point_debut.code, point_fin.code, code_potentiometre);
            }
        }
    }
    if (erreur_minimale > calibration_forme->erreur_amplitude_mvpp)
    {
        return GF_PLAGE;
    }
    int32_t delta_offset_demande_mv = signal->offset_mv - niveau_zero_mv;
    if (!entre(delta_offset_demande_mv, calibration_forme->offset[0].delta_mv, calibration_forme->offset[calibration_forme->nombre_offset - 1].delta_mv))
    {
        return GF_PLAGE;
    }
    erreur_minimale = INT64_MAX;
    for (unsigned index_segment = 1; index_segment < calibration_forme->nombre_offset; index_segment++)
    {
        for (unsigned code_potentiometre = calibration_forme->offset[index_segment - 1].code; code_potentiometre <= calibration_forme->offset[index_segment].code; code_potentiometre++)
        {
            const cal_offset_t point_debut = calibration_forme->offset[index_segment - 1], point_fin = calibration_forme->offset[index_segment];
            int32_t offset_estime_mv = niveau_zero_mv + interp(point_debut.delta_mv, point_fin.delta_mv, point_debut.code, point_fin.code, code_potentiometre);
            int64_t ecart_absolu = distance(offset_estime_mv, signal->offset_mv);
            if (ecart_absolu < erreur_minimale)
            {
                erreur_minimale = ecart_absolu;
                meilleur_reglage.offset =(uint8_t) code_potentiometre;
                meilleur_reglage.offset_estime_mv = offset_estime_mv;
            }
        }
    }
    if (erreur_minimale > calibration_forme->erreur_offset_mv || !excursion(profil, meilleur_reglage.amplitude_estimee_mvpp, meilleur_reglage.offset_estime_mv))
    {
        return GF_PLAGE;
    }
    * commande = meilleur_reglage;
    return GF_OK;
}

/* Mode nominal NON MESURE : 600 mVpp apres C1/R11, U1=50, sommateur x1.
* Les 320 mV continus de l'AD9833 sont bloques EN REGIME ETABLI par C1.
* Les effets de polarisation, bandes passantes, charge et LT1206 restent inconnus. */
/*--------------------------------------------------------------------------------------------------
 * Fonction   : analogique_convertir_nominal()
 * Descriptif : Calcule une estimation non mesuree sur VOUT.
 *--------------------------------------------------------------------------------------------------*/
gf_result_t analogique_convertir_nominal(const signal_config_t *signal, analogique_commande_t *commande)
{
    if (!signal || !commande || !signaux_valide(*signal))
    {
        return GF_ARGUMENT;
    }
    const int64_t amplitude_max_mvpp = CAL_NOMINAL_DDS_AMPL_MVPP * CAL_NOMINAL_ETAGE_GAIN, rail_mv = CAL_NOMINAL_RAIL_MV, plage_offset_mv = 2 * rail_mv;
    /* Garde mathematique des rails +/-15 V (pas une garantie de swing reel). */
    if (2 *(int64_t) signal->offset_mv - signal->amplitude_mvpp < - 2 * rail_mv || 2 *(int64_t) signal->offset_mv + signal->amplitude_mvpp > 2 * rail_mv)
    {
        return GF_PLAGE;
    }
    int64_t gain =((int64_t) signal->amplitude_mvpp * CAL_EPOT_CODE_MAX + amplitude_max_mvpp / 2) / amplitude_max_mvpp;
    int64_t offset =(((int64_t) signal->offset_mv + rail_mv) * CAL_EPOT_CODE_MAX + plage_offset_mv / 2) / plage_offset_mv;
    if (gain < 0 || gain > CAL_EPOT_CODE_MAX || offset < 0 || offset > CAL_EPOT_CODE_MAX)
    {
        return GF_PLAGE;
    }
    analogique_commande_t reglage_nominal = { 0 };
    reglage_nominal.gain =(uint8_t) gain;
    reglage_nominal.offset =(uint8_t) offset;
    reglage_nominal.amplitude_estimee_mvpp =(int32_t)((gain * amplitude_max_mvpp + (CAL_EPOT_CODE_MAX / 2U)) / CAL_EPOT_CODE_MAX);
    reglage_nominal.offset_estime_mv =(int32_t)(-rail_mv +(offset * plage_offset_mv + (CAL_EPOT_CODE_MAX / 2U)) / CAL_EPOT_CODE_MAX);
    if (2 *(int64_t) reglage_nominal.offset_estime_mv - reglage_nominal.amplitude_estimee_mvpp < - 2 * rail_mv || 2 *(int64_t) reglage_nominal.offset_estime_mv + reglage_nominal.amplitude_estimee_mvpp > 2 * rail_mv)
    {
        return GF_PLAGE;
    }
    * commande = reglage_nominal;
    return GF_OK;
}

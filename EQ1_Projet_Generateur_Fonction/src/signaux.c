/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : SIGNAUX.C
 * DESCRIPTIF : Catalogue des formes d'ondes, limites et formats.
 *==================================================================================================*/
#include "signaux.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/*============================== VARIABLES ET CONFIGURATION =======================================*/
#define SIGNAUX_MV_PAR_VOLT              1000LL
#define SIGNAUX_HZ_PAR_KHZ               1000U
#define SIGNAUX_HZ_PAR_MHZ               1000000U
#define SIGNAUX_PAS_AMPLITUDE_DEFAUT     100U
#define SIGNAUX_PAS_FREQUENCE_DEFAUT     1U
static signal_config_t catalogue[SIGNAL_MAX];
static bool profils_modifies[SIGNAL_MAX];
static size_t nombre_signaux;
static bool catalogue_modifie;
/* Les dix valeurs du cahier des charges, en mVpp / mV / Hz. */
static const signal_config_t profils_usine[SIGNAL_INIT_COUNT] = {
    { SIGNAL_SINUS, 1000, 0, 10000 },
    { SIGNAL_SINUS, 5000, 0, 1000000 },
    { SIGNAL_SINUS, 2000, 3000, 1000 },
    { SIGNAL_SINUS, 200, 10000, 500 },
    { SIGNAL_TRIANGLE, 20000, 0, 5000 },
    { SIGNAL_TRIANGLE, 4000, 0, 100 },
    { SIGNAL_TRIANGLE, 5000, -5000, 200000 },
    { SIGNAL_CARRE, 10000, 0, 50 },
    { SIGNAL_CARRE, 1000, 0, 300 },
    { SIGNAL_CARRE, 4000, 6000, 1000 }
};
/*==================================== FONCTIONS ================================================*/
/*================================ INTERFACE PUBLIQUE ============================================*/
/*--------------------------------------------------------------------------------------------------
 * Fonction   : signaux_initialiser()
 * Descriptif : Charge les dix profils fournis dans le cahier des charges.
 *--------------------------------------------------------------------------------------------------*/
void signaux_initialiser(void)
{
    memset(catalogue, 0, sizeof(catalogue));
    memset(profils_modifies, 0, sizeof(profils_modifies));
    memcpy(catalogue, profils_usine, sizeof(profils_usine));
    nombre_signaux = SIGNAL_INIT_COUNT;
    catalogue_modifie = false;
}

void signaux_retablir_usine(void)
{
    signaux_initialiser();
    catalogue_modifie = true;
}

size_t signaux_nombre(void)
{
    return nombre_signaux;
}

const signal_config_t *signaux_lire(size_t index_signal)
{
    if (index_signal < nombre_signaux)
    {
        return&catalogue[index_signal];
    }
    return NULL;
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : signaux_valide()
 * Descriptif : Controle les limites de chaque parametre d'un signal.
 *--------------------------------------------------------------------------------------------------*/
bool signaux_valide(signal_config_t s)
{
    return s.forme >= SIGNAL_SINUS && s.forme <= SIGNAL_CARRE && s.amplitude_mvpp >= SIGNAL_AMPL_MIN_MVPP && s.amplitude_mvpp <= SIGNAL_AMPL_MAX_MVPP
        && s.offset_mv >= SIGNAL_OFFSET_MIN_MV && s.offset_mv <= SIGNAL_OFFSET_MAX_MV && s.frequence_hz >= SIGNAL_FREQ_MIN_HZ
        && s.frequence_hz <= SIGNAL_FREQ_MAX_HZ;
}

bool signaux_remplacer_ensemble(const signal_config_t *donnees, size_t n)
{
    if (!donnees || n == 0 || n > SIGNAL_MAX)
    {
        return false;
    }
    for (size_t index_signal = 0; index_signal < n; index_signal++)
    {
        if (!signaux_valide(donnees[index_signal]))
        {
            return false;
        }
    }
    memmove(catalogue, donnees, n * sizeof(*donnees));
    if (n < SIGNAL_MAX)
    {
        memset(catalogue + n, 0, (SIGNAL_MAX - n) * sizeof(*donnees));
    }
    memset(profils_modifies, 0, sizeof(profils_modifies));
    nombre_signaux = n;
    catalogue_modifie = false;
    return true;
}

bool signaux_modifier(size_t index_signal, signal_config_t s)
{
    if (index_signal >= nombre_signaux || !signaux_valide(s))
    {
        return false;
    }
    if (catalogue[index_signal].forme != s.forme || catalogue[index_signal].amplitude_mvpp != s.amplitude_mvpp || catalogue[index_signal].offset_mv != s.offset_mv
        || catalogue[index_signal].frequence_hz != s.frequence_hz)
    {
        catalogue[index_signal] = s;
        profils_modifies[index_signal] = true;
    }
    return true;
}

int signaux_dupliquer(size_t index_signal)
{
    if (index_signal >= nombre_signaux || nombre_signaux >= SIGNAL_MAX)
    {
        return- 1;
    }
    catalogue[nombre_signaux] = catalogue[index_signal];
    profils_modifies[nombre_signaux] = true;
    catalogue_modifie = true;
    return(int) nombre_signaux++;
}

bool signaux_supprimer(size_t index_signal)
{
    if (index_signal >= nombre_signaux || nombre_signaux <= 1)
    {
        return false;
    }
    for (size_t j = index_signal; j + 1 < nombre_signaux; j++)
    {
        catalogue[j] = catalogue[j + 1];
        profils_modifies[j] = profils_modifies[j + 1];
    }
    nombre_signaux--;
    memset(&catalogue[nombre_signaux], 0, sizeof(catalogue[nombre_signaux]));
    profils_modifies[nombre_signaux] = false;
    catalogue_modifie = true;
    return true;
}

bool signaux_modifie(size_t index_signal)
{
    return index_signal < nombre_signaux && profils_modifies[index_signal];
}

size_t signaux_nombre_modifies(void)
{
    size_t n = 0;
    for (size_t index_signal = 0; index_signal < nombre_signaux; index_signal++)
    {
        if (profils_modifies[index_signal])
        {
            n++;
        }
    }
    return n;
}

bool signaux_modifications_en_ram(void)
{
    return catalogue_modifie || signaux_nombre_modifies() != 0;
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : signaux_variation()
 * Descriptif : Modifie une valeur tout en respectant ses limites.
 *--------------------------------------------------------------------------------------------------*/
void signaux_variation(signal_config_t *signal, signal_champ_t champ, int direction, uint32_t pas)
{
    if (!signal || !direction)
    {
        return;
    }
    int sens;
    if (direction > 0)
    {
        sens = 1;
    }
    else
    {
        sens = -1;
    }
    if (champ == CHAMP_FORME)
    {
        int forme_demandee =(int) signal->forme + sens;
        int forme = forme_demandee;
        if (forme_demandee < 0)
        {
            forme = 2;
        }
        else if (forme_demandee > 2)
        {
            forme = 0;
        }
        signal->forme =(signal_forme_t) forme;
    }
    else if (champ == CHAMP_AMPLITUDE || champ == CHAMP_OFFSET)
    {
        int64_t valeur;
        if (champ == CHAMP_AMPLITUDE)
        {
            valeur = signal->amplitude_mvpp;
        }
        else
        {
            valeur = signal->offset_mv;
        }
        int64_t mini;
        if (champ == CHAMP_AMPLITUDE)
        {
            mini = SIGNAL_AMPL_MIN_MVPP;
        }
        else
        {
            mini = SIGNAL_OFFSET_MIN_MV;
        }
        int64_t maxi;
        if (champ == CHAMP_AMPLITUDE)
        {
            maxi = SIGNAL_AMPL_MAX_MVPP;
        }
        else
        {
            maxi = SIGNAL_OFFSET_MAX_MV;
        }
        uint32_t pas_effectif = SIGNAUX_PAS_AMPLITUDE_DEFAUT;
        if (pas)
        {
            pas_effectif = pas;
        }
        valeur +=(int64_t) sens *(int64_t) pas_effectif;
        if (valeur < mini)
        {
            valeur = mini;
        }
        if (valeur > maxi)
        {
            valeur = maxi;
        }
        if (champ == CHAMP_AMPLITUDE)
        {
            signal->amplitude_mvpp =(int32_t) valeur;
        }
        else
        {
            signal->offset_mv =(int32_t) valeur;
        }
    }
    else if (champ == CHAMP_FREQUENCE)
    {
        uint32_t pas_effectif = SIGNAUX_PAS_FREQUENCE_DEFAUT;
        if (pas)
        {
            pas_effectif = pas;
        }
        int64_t valeur =(int64_t) signal->frequence_hz +(int64_t) sens *(int64_t) pas_effectif;
        if (valeur < SIGNAL_FREQ_MIN_HZ)
        {
            valeur = SIGNAL_FREQ_MIN_HZ;
        }
        if (valeur > SIGNAL_FREQ_MAX_HZ)
        {
            valeur = SIGNAL_FREQ_MAX_HZ;
        }
        signal->frequence_hz =(uint32_t) valeur;
    }
}

const char *signaux_type_court(signal_forme_t s)
{
    switch (s)
    {
    case SIGNAL_SINUS:
        return "SIN";
    case SIGNAL_TRIANGLE:
        return "TRI";
    case SIGNAL_CARRE:
        return "CAR";
    default:
        return "???";
    }
}

const char *signaux_type_long(signal_forme_t s)
{
    switch (s)
    {
    case SIGNAL_SINUS:
        return "SINUS";
    case SIGNAL_TRIANGLE:
        return "TRIANGLE";
    case SIGNAL_CARRE:
        return "CARRE";
    default:
        return "INVALIDE";
    }
}

void signaux_formater_tension(char *destination, size_t capacite_destination, int32_t millivolts, bool avec_signe)
{
    if (!destination || !capacite_destination)
    {
        return;
    }
    int64_t valeur_absolue_mv = millivolts;
    char symbole_signe = '+';
    if (valeur_absolue_mv < 0)
    {
        symbole_signe = '-';
    }
    if (valeur_absolue_mv < 0)
    {
        valeur_absolue_mv = -valeur_absolue_mv;
    }
    if (avec_signe)
    {
        snprintf(destination, capacite_destination, "%c%" PRId64 ".%03" PRId64 "V", symbole_signe, valeur_absolue_mv / SIGNAUX_MV_PAR_VOLT, valeur_absolue_mv % SIGNAUX_MV_PAR_VOLT);
    }
    else
    {
        snprintf(destination, capacite_destination, "%" PRId64 ".%03" PRId64 "V", valeur_absolue_mv / SIGNAUX_MV_PAR_VOLT, valeur_absolue_mv % SIGNAUX_MV_PAR_VOLT);
    }
}

void signaux_formater_frequence(char *destination, size_t capacite_destination, uint32_t frequence_hz)
{
    if (!destination || !capacite_destination)
    {
        return;
    }
    if (frequence_hz >= SIGNAUX_HZ_PAR_MHZ && frequence_hz % SIGNAUX_HZ_PAR_MHZ == 0)
    {
        snprintf(destination, capacite_destination, "%" PRIu32 "MHz", frequence_hz / SIGNAUX_HZ_PAR_MHZ);
    }
    else if (frequence_hz >= SIGNAUX_HZ_PAR_KHZ && frequence_hz % SIGNAUX_HZ_PAR_KHZ == 0)
    {
        snprintf(destination, capacite_destination, "%" PRIu32 "kHz", frequence_hz / SIGNAUX_HZ_PAR_KHZ);
    }
    else
    {
        snprintf(destination, capacite_destination, "%" PRIu32 "Hz", frequence_hz);
    }
}

bool signaux_ajouter(signal_config_t s)
{
    if (nombre_signaux >= SIGNAL_MAX || !signaux_valide(s))
    {
        return false;
    }
    catalogue[nombre_signaux] = s;
    profils_modifies[nombre_signaux++] = true;
    catalogue_modifie = true;
    return true;
}

void signaux_marquer_sauves(void)
{
    memset(profils_modifies, 0, sizeof(profils_modifies));
    catalogue_modifie = false;
}

bool signaux_egaux(signal_config_t a, signal_config_t b)
{
    return a.forme == b.forme && a.amplitude_mvpp == b.amplitude_mvpp && a.offset_mv == b.offset_mv && a.frequence_hz == b.frequence_hz;
}

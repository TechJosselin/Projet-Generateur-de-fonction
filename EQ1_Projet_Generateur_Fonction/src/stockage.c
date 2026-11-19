/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : STOCKAGE.C
 * DESCRIPTIF : Sauvegarde EEPROM sur deux banques et controle CRC.
 *==================================================================================================*/
#include "stockage.h"
#include <string.h>

/*============================== VARIABLES ET CONFIGURATION =======================================*/
/* Format GFN2 : offsets et tailles figes, identiques en lecture et ecriture. */
#define STOCKAGE_SIGNATURE                 "GFN2"
#define STOCKAGE_SIGNATURE_OCTETS           4U
#define STOCKAGE_MARQUEUR_VALIDE            0xA5U
#define STOCKAGE_TAILLE_ENTETE_OCTETS      32U
#define STOCKAGE_TAILLE_SIGNAL_OCTETS      13U
#define STOCKAGE_NOMBRE_BANQUES             2U
#define STOCKAGE_EEPROM_OCTET_EFFACE       UINT8_MAX
#define STOCKAGE_POSITION_VERSION           4U
#define STOCKAGE_POSITION_LONGUEUR          6U
#define STOCKAGE_POSITION_GENERATION        8U
#define STOCKAGE_POSITION_NOMBRE_SIGNAUX   12U
#define STOCKAGE_POSITION_TAILLE_CAL       14U
#define STOCKAGE_POSITION_CRC_DONNEES      20U
#define STOCKAGE_POSITION_CRC_ENTETE       24U
#define STOCKAGE_CRC_ENTETE_OCTETS         24U
#define STOCKAGE_POSITION_MARQUEUR         (STOCKAGE_TAILLE_ENTETE_OCTETS - 1U)
#define STOCKAGE_ENTETE_AVANT_MARQUEUR     (STOCKAGE_TAILLE_ENTETE_OCTETS - 1U)
#define STOCKAGE_POSITION_SIGNAL_FORME      0U
#define STOCKAGE_POSITION_SIGNAL_AMPL       1U
#define STOCKAGE_POSITION_SIGNAL_OFFSET     5U
#define STOCKAGE_POSITION_SIGNAL_FREQ       9U
#define STOCKAGE_DEMI_PLAGE_GENERATIONS     UINT32_C(0x80000000)
#define STOCKAGE_ATTENTE_ECRITURE_MAX_MS   50U
_Static_assert(STOCKAGE_TAILLE_ENTETE_OCTETS + SIGNAL_MAX * STOCKAGE_TAILLE_SIGNAL_OCTETS + CAL_IMAGE_SIZE <= STOCKAGE_BANQUE, "EEPROM trop petite");
enum
{
    REPOS = 0, INVALIDER, CONTROLER_INVALIDE, CORPS, HEADER, VERIFIER, VALIDER, CONTROLER_VALIDE
};
/*==================================== FONCTIONS ================================================*/
/*================================ INTERFACE PUBLIQUE ============================================*/
void stockage_initialiser(stockage_t *etat_stockage, stockage_io_t io)
{
    memset(etat_stockage, 0, sizeof(*etat_stockage));
    etat_stockage->io = io;
    etat_stockage->actif = -1;
    etat_stockage->cible = -1;
}

bool stockage_occupe(const stockage_t *etat_stockage)
{
    return etat_stockage && etat_stockage->etape != REPOS;
}

static bool plus_recent(uint32_t generation_nouvelle, uint32_t image_binaire)
{
    uint32_t ecart_generations = generation_nouvelle - image_binaire;
    return ecart_generations != 0 && ecart_generations < STOCKAGE_DEMI_PLAGE_GENERATIONS;
}

static gf_result_t decoder(const uint8_t *image_binaire, stockage_donnees_t *donnees, uint32_t *generation_lue)
{
    if (image_binaire[STOCKAGE_POSITION_MARQUEUR] != STOCKAGE_MARQUEUR_VALIDE)
    {
        return GF_CRC;
    }
    if (memcmp(image_binaire, STOCKAGE_SIGNATURE, STOCKAGE_SIGNATURE_OCTETS) || gf_u32(image_binaire + STOCKAGE_POSITION_CRC_ENTETE) != gf_crc32(image_binaire, STOCKAGE_CRC_ENTETE_OCTETS))
    {
        return GF_CRC;
    }
    if (gf_u16(image_binaire + STOCKAGE_POSITION_VERSION) != STOCKAGE_VERSION)
    {
        return GF_VERSION;
    }
    size_t nombre_signaux = gf_u16(image_binaire + STOCKAGE_POSITION_NOMBRE_SIGNAUX), longueur_donnees = gf_u16(image_binaire + STOCKAGE_POSITION_LONGUEUR);
    if (!nombre_signaux || nombre_signaux > SIGNAL_MAX || gf_u16(image_binaire + STOCKAGE_POSITION_TAILLE_CAL) != CAL_IMAGE_SIZE || longueur_donnees != nombre_signaux * STOCKAGE_TAILLE_SIGNAL_OCTETS + CAL_IMAGE_SIZE || longueur_donnees > STOCKAGE_BANQUE - STOCKAGE_TAILLE_ENTETE_OCTETS)
    {
        return GF_CRC;
    }
    if (gf_u32(image_binaire + STOCKAGE_POSITION_CRC_DONNEES) != gf_crc32(image_binaire + STOCKAGE_TAILLE_ENTETE_OCTETS, longueur_donnees))
    {
        return GF_CRC;
    }
    for (size_t index_element = 0; index_element < nombre_signaux; index_element++)
    {
        const uint8_t *enregistrement = image_binaire + STOCKAGE_TAILLE_ENTETE_OCTETS + index_element * STOCKAGE_TAILLE_SIGNAL_OCTETS;
        signal_config_t signal_decode = {
            (signal_forme_t) enregistrement[STOCKAGE_POSITION_SIGNAL_FORME],
            gf_i32(enregistrement + STOCKAGE_POSITION_SIGNAL_AMPL),
            gf_i32(enregistrement + STOCKAGE_POSITION_SIGNAL_OFFSET),
            gf_u32(enregistrement + STOCKAGE_POSITION_SIGNAL_FREQ)
        };
        if (!signaux_valide(signal_decode))
        {
            return GF_CRC;
        }
        donnees->signaux[index_element] = signal_decode;
    }
    gf_result_t resultat = calibration_decoder(image_binaire + STOCKAGE_TAILLE_ENTETE_OCTETS + nombre_signaux * STOCKAGE_TAILLE_SIGNAL_OCTETS, &donnees->calibration);
    if (resultat != GF_OK)
    {
        return resultat;
    }
    donnees->nombre = nombre_signaux;
    * generation_lue = gf_u32(image_binaire + STOCKAGE_POSITION_GENERATION);
    return GF_OK;
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : stockage_charger()
 * Descriptif : Selectionne la banque EEPROM valide la plus recente.
 *--------------------------------------------------------------------------------------------------*/
gf_result_t stockage_charger(stockage_t *etat_stockage, stockage_donnees_t *donnees)
{
    if (!etat_stockage || !donnees)
    {
        return GF_ARGUMENT;
    }
    if (stockage_occupe(etat_stockage))
    {
        return GF_OCCUPE;
    }
    if (!etat_stockage->io.read)
    {
        return GF_ABSENT;
    }
    stockage_donnees_t donnees_banque;
    bool banque_valide_trouvee = false, tout_vierge = true;
    gf_result_t erreur = GF_CRC;
    int actif = -1;
    uint32_t generation = 0;
    etat_stockage->alerte = GF_OK;
    for (unsigned banque = 0; banque < STOCKAGE_NOMBRE_BANQUES; banque++)
    {
        gf_result_t resultat = etat_stockage->io.read(etat_stockage->io.ctx, (uint16_t)(banque * STOCKAGE_BANQUE), etat_stockage->image, STOCKAGE_BANQUE);
        if (resultat != GF_OK)
        {
            tout_vierge = false;
            erreur = resultat;
            etat_stockage->alerte = resultat;
            continue;
        }
        bool vierge = true;
        for (size_t index_element = 0; index_element < STOCKAGE_BANQUE; index_element++)
        {
            if (etat_stockage->image[index_element] != STOCKAGE_EEPROM_OCTET_EFFACE)
            {
                vierge = false;
                break;
            }
        }
        tout_vierge = tout_vierge && vierge;
        if (vierge)
        {
            continue;
        }
        uint32_t generation_lue = 0;
        resultat = decoder(etat_stockage->image, &donnees_banque, &generation_lue);
        if (resultat != GF_OK)
        {
            erreur = resultat;
            etat_stockage->alerte = resultat;
            continue;
        }
        if (!banque_valide_trouvee || plus_recent(generation_lue, generation))
        {
            *donnees = donnees_banque;
            generation = generation_lue;
            actif =(int) banque;
            banque_valide_trouvee = true;
        }
    }
    if (banque_valide_trouvee)
    {
        etat_stockage->actif = actif;
        etat_stockage->generation = generation;
        return GF_OK;
    }
    etat_stockage->actif = -1;
    etat_stockage->generation = 0;
    if (tout_vierge)
    {
        return GF_VIDE;
    }
    return erreur;
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : stockage_commencer()
 * Descriptif : Prepare une sauvegarde dans la banque inactive.
 *--------------------------------------------------------------------------------------------------*/
gf_result_t stockage_commencer(stockage_t *etat_stockage, const stockage_donnees_t *donnees)
{
    if (!etat_stockage || !donnees || !donnees->nombre || donnees->nombre > SIGNAL_MAX)
    {
        return GF_ARGUMENT;
    }
    if (stockage_occupe(etat_stockage))
    {
        return GF_OCCUPE;
    }
    if (!etat_stockage->io.read || !etat_stockage->io.page_write || !etat_stockage->io.ready)
    {
        return GF_ABSENT;
    }
    for (size_t index_element = 0; index_element < donnees->nombre; index_element++)
    {
        if (!signaux_valide(donnees->signaux[index_element]))
        {
            return GF_ARGUMENT;
        }
    }
    uint8_t *image_binaire = etat_stockage->image;
    memset(image_binaire, STOCKAGE_EEPROM_OCTET_EFFACE, STOCKAGE_BANQUE);
    memset(image_binaire, 0, STOCKAGE_TAILLE_ENTETE_OCTETS);
    memcpy(image_binaire, STOCKAGE_SIGNATURE, STOCKAGE_SIGNATURE_OCTETS);
    gf_p16(image_binaire + STOCKAGE_POSITION_VERSION, STOCKAGE_VERSION);
    etat_stockage->longueur = STOCKAGE_TAILLE_ENTETE_OCTETS + donnees->nombre * STOCKAGE_TAILLE_SIGNAL_OCTETS + CAL_IMAGE_SIZE;
    gf_p16(image_binaire + STOCKAGE_POSITION_LONGUEUR, (uint16_t)(etat_stockage->longueur - STOCKAGE_TAILLE_ENTETE_OCTETS));
    etat_stockage->generation_cible = etat_stockage->generation + 1;
    gf_p32(image_binaire + STOCKAGE_POSITION_GENERATION, etat_stockage->generation_cible);
    gf_p16(image_binaire + STOCKAGE_POSITION_NOMBRE_SIGNAUX, (uint16_t) donnees->nombre);
    gf_p16(image_binaire + STOCKAGE_POSITION_TAILLE_CAL, CAL_IMAGE_SIZE);
    for (size_t index_element = 0; index_element < donnees->nombre; index_element++)
    {
        uint8_t *enregistrement = image_binaire + STOCKAGE_TAILLE_ENTETE_OCTETS + index_element * STOCKAGE_TAILLE_SIGNAL_OCTETS;
        const signal_config_t *signal_a_encoder =&donnees->signaux[index_element];
        enregistrement[STOCKAGE_POSITION_SIGNAL_FORME] =(uint8_t) signal_a_encoder->forme;
        gf_p32(enregistrement + STOCKAGE_POSITION_SIGNAL_AMPL, (uint32_t) signal_a_encoder->amplitude_mvpp);
        gf_p32(enregistrement + STOCKAGE_POSITION_SIGNAL_OFFSET, (uint32_t) signal_a_encoder->offset_mv);
        gf_p32(enregistrement + STOCKAGE_POSITION_SIGNAL_FREQ, signal_a_encoder->frequence_hz);
    }
    gf_result_t resultat = calibration_encoder(&donnees->calibration, image_binaire + STOCKAGE_TAILLE_ENTETE_OCTETS + donnees->nombre * STOCKAGE_TAILLE_SIGNAL_OCTETS);
    if (resultat != GF_OK)
    {
        return resultat;
    }
    gf_p32(image_binaire + STOCKAGE_POSITION_CRC_DONNEES, gf_crc32(image_binaire + STOCKAGE_TAILLE_ENTETE_OCTETS, etat_stockage->longueur - STOCKAGE_TAILLE_ENTETE_OCTETS));
    gf_p32(image_binaire + STOCKAGE_POSITION_CRC_ENTETE, gf_crc32(image_binaire, STOCKAGE_CRC_ENTETE_OCTETS));
    etat_stockage->cible = 0;
    if (etat_stockage->actif == 0)
    {
        etat_stockage->cible = 1;
    }
    etat_stockage->position = STOCKAGE_TAILLE_ENTETE_OCTETS;
    etat_stockage->etape = INVALIDER;
    etat_stockage->attend = false;
    etat_stockage->resultat = GF_OK;
    return GF_OK;
}

static void terminer(stockage_t *etat_stockage, gf_result_t resultat)
{
    etat_stockage->resultat = resultat;
    etat_stockage->etape = REPOS;
    etat_stockage->attend = false;
}

static bool ecrire(stockage_t *etat_stockage, size_t position, const uint8_t *donnees_page, size_t longueur_page, uint32_t maintenant_ms)
{
    gf_result_t resultat = etat_stockage->io.page_write(etat_stockage->io.ctx, (uint16_t)(etat_stockage->cible * STOCKAGE_BANQUE + position), donnees_page, longueur_page);
    if (resultat != GF_OK)
    {
        terminer(etat_stockage, resultat);
        return false;
    }
    etat_stockage->attend = true;
    etat_stockage->attente_depuis = maintenant_ms;
    return true;
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : stockage_pas()
 * Descriptif : Execute les etapes d'ecriture et de verification.
 *--------------------------------------------------------------------------------------------------*/
void stockage_pas(stockage_t *etat_stockage, uint32_t maintenant_ms)
{
    if (!stockage_occupe(etat_stockage))
    {
        return;
    }
    if (etat_stockage->attend)
    {
        gf_result_t resultat = etat_stockage->io.ready(etat_stockage->io.ctx);
        if (resultat == GF_OCCUPE)
        {
            if ((uint32_t)(maintenant_ms - etat_stockage->attente_depuis) >= STOCKAGE_ATTENTE_ECRITURE_MAX_MS)
            {
                terminer(etat_stockage, GF_TIMEOUT);
            }
            return;
        }
        if (resultat != GF_OK)
        {
            terminer(etat_stockage, resultat);
            return;
        }
        etat_stockage->attend = false;
        return;
        /* Une operation IO maximum par iteration. */
    }
    uint8_t tampon_page[STOCKAGE_PAGE];
    gf_result_t resultat;
    size_t longueur_page;
    switch (etat_stockage->etape)
    {
    case INVALIDER:
        tampon_page[0] = 0;
        if (ecrire(etat_stockage, STOCKAGE_POSITION_MARQUEUR, tampon_page, 1, maintenant_ms))
        {
            etat_stockage->etape = CONTROLER_INVALIDE;
        }
        break;
    case CONTROLER_INVALIDE:
        resultat = etat_stockage->io.read(etat_stockage->io.ctx, (uint16_t)(etat_stockage->cible * STOCKAGE_BANQUE + STOCKAGE_POSITION_MARQUEUR), tampon_page, 1);
        if (resultat != GF_OK || tampon_page[0] != 0)
        {
            gf_result_t cause = GF_VERIFICATION;
            if (resultat != GF_OK)
            {
                cause = resultat;
            }
            terminer(etat_stockage, cause);
            break;
        }
        etat_stockage->etape = CORPS;
        break;
    case CORPS:
        longueur_page = etat_stockage->longueur - etat_stockage->position;
        if (longueur_page > STOCKAGE_PAGE)
        {
            longueur_page = STOCKAGE_PAGE;
        }
        if (ecrire(etat_stockage, etat_stockage->position, etat_stockage->image + etat_stockage->position, longueur_page, maintenant_ms))
        {
            etat_stockage->position += longueur_page;
            if (etat_stockage->position == etat_stockage->longueur)
            {
                etat_stockage->etape = HEADER;
            }
        }
        break;
    case HEADER:
        if (ecrire(etat_stockage, 0, etat_stockage->image, STOCKAGE_ENTETE_AVANT_MARQUEUR, maintenant_ms))
        {
            etat_stockage->position = 0;
            etat_stockage->etape = VERIFIER;
        }
        break;
    case VERIFIER:
        longueur_page = etat_stockage->longueur - etat_stockage->position;
        if (longueur_page > sizeof(tampon_page))
        {
            longueur_page = sizeof(tampon_page);
        }
        resultat = etat_stockage->io.read(etat_stockage->io.ctx, (uint16_t)(etat_stockage->cible * STOCKAGE_BANQUE + etat_stockage->position), tampon_page, longueur_page);
        if (resultat != GF_OK || memcmp(tampon_page, etat_stockage->image + etat_stockage->position, longueur_page))
        {
            gf_result_t cause = GF_VERIFICATION;
            if (resultat != GF_OK)
            {
                cause = resultat;
            }
            terminer(etat_stockage, cause);
            break;
        }
        etat_stockage->position += longueur_page;
        if (etat_stockage->position == etat_stockage->longueur)
        {
            etat_stockage->etape = VALIDER;
        }
        break;
    case VALIDER:
        tampon_page[0] = STOCKAGE_MARQUEUR_VALIDE;
        if (ecrire(etat_stockage, STOCKAGE_POSITION_MARQUEUR, tampon_page, 1, maintenant_ms))
        {
            etat_stockage->etape = CONTROLER_VALIDE;
        }
        break;
    case CONTROLER_VALIDE:
        resultat = etat_stockage->io.read(etat_stockage->io.ctx, (uint16_t)(etat_stockage->cible * STOCKAGE_BANQUE), tampon_page, STOCKAGE_TAILLE_ENTETE_OCTETS);
        etat_stockage->image[STOCKAGE_POSITION_MARQUEUR] = STOCKAGE_MARQUEUR_VALIDE;
        if (resultat != GF_OK || memcmp(tampon_page, etat_stockage->image, STOCKAGE_TAILLE_ENTETE_OCTETS))
        {
            gf_result_t cause = GF_VERIFICATION;
            if (resultat != GF_OK)
            {
                cause = resultat;
            }
            terminer(etat_stockage, cause);
            break;
        }
        etat_stockage->actif = etat_stockage->cible;
        etat_stockage->generation = etat_stockage->generation_cible;
        terminer(etat_stockage, GF_OK);
        break;
    default:
        terminer(etat_stockage, GF_ARGUMENT);
        break;
    }
}

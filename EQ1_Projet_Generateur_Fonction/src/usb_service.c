/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : USB_SERVICE.C
 * DESCRIPTIF : Traitement des commandes et mise en file des reponses.
 *==================================================================================================*/
#include "usb_service.h"
#include "integration.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/*============================== VARIABLES ET CONFIGURATION =======================================*/
#define USB_SERVICE_REPONSE_MAX_OCTETS           192U
#define USB_SERVICE_TRAME_SIGNAL_MAX_OCTETS      64U
#define USB_SERVICE_REPONSE_ERREUR_OCTETS        64U
#define USB_SERVICE_REPONSE_OK_OCTETS            80U
#define USB_SERVICE_CAL_RESERVE_TX_OCTETS       2048U
#define USB_SERVICE_TIMEOUT_RX_MS               1000U
#define USB_SERVICE_TIMEOUT_TX_MS               2000U
#define USB_SERVICE_TIMEOUT_CAL_MS             30000U
#define USB_SERVICE_ENVOI_MAX_OCTETS             64U
#define USB_SERVICE_CARACTERE_ASCII_MIN          32U
#define USB_SERVICE_CARACTERE_ASCII_MAX         126U
static uint8_t buffer_tx[USB_TX_CAPACITE], image_calibration[CAL_IMAGE_SIZE];
static char buffer_rx[USB_TRAME_MAX];
static size_t debut_tx, octets_a_emettre, longueur_rx;
static uint32_t dernier_octet_rx_ms, dernier_octet_tx_ms, horloge_usb_ms, dernier_bloc_cal_ms, revision_calibration;
static uint16_t masque_blocs_cal_recus;
static bool calibration_en_cours, reinitialiser_transport;
static usb_stats_t stats;
/*==================================== FONCTIONS ================================================*/
/*================================ INTERFACE PUBLIQUE ============================================*/
void usb_service_initialiser(void)
{
    debut_tx = octets_a_emettre = longueur_rx = 0;
    dernier_octet_rx_ms = dernier_octet_tx_ms = horloge_usb_ms = dernier_bloc_cal_ms = revision_calibration = 0;
    masque_blocs_cal_recus = 0;
    calibration_en_cours = reinitialiser_transport = false;
    memset(&stats, 0, sizeof(stats));
}

const usb_stats_t *usb_service_statistiques(void)
{
    return&stats;
}

size_t usb_service_espace_tx(void)
{
    return USB_TX_CAPACITE - octets_a_emettre;
}

static void emettre(const char *s)
{
    size_t n = strlen(s);
    if (n > usb_service_espace_tx())
    {
        stats.abandons_tx++;
        return;
    }
    if (!octets_a_emettre)
    {
        dernier_octet_tx_ms = horloge_usb_ms;
    }
    for (size_t i = 0; i < n; i++)
    {
        buffer_tx[(debut_tx + octets_a_emettre + i) % USB_TX_CAPACITE] =(uint8_t) s[i];
    }
    octets_a_emettre += n;
}

static void erreur(gf_result_t r)
{
    char b[USB_SERVICE_REPONSE_ERREUR_OCTETS];
    snprintf(b, sizeof(b), "$ERR:%s#", gf_result_nom(r));
    emettre(b);
}

static void ok(gf_result_t r, const char *action)
{
    if (r != GF_OK)
    {
        erreur(r);
        return;
    }
    char b[USB_SERVICE_REPONSE_OK_OCTETS];
    snprintf(b, sizeof(b), "$OK:%s:%" PRIu32 "#", action, integration_statut()->revision);
    emettre(b);
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : executer()
 * Descriptif : Distribue une commande au sous-systeme concerne.
 *--------------------------------------------------------------------------------------------------*/
static void executer(const usb_commande_t *commande)
{
    char reponse[USB_SERVICE_REPONSE_MAX_OCTETS];
    gf_result_t resultat = GF_OK;
    /* Reserve une reponse avant toute mutation, pas d'action silencieuse. */
    if (usb_service_espace_tx() < sizeof(reponse))
    {
        erreur(GF_OCCUPE);
        return;
    }
    switch (commande->action)
    {
    case USB_ECRIRE:
        ok(integration_modifier(commande->index, commande->signal, 0), "RAM");
        break;
    case USB_LIRE:
        if (usb_proto_formater_signal(commande->index, reponse, sizeof(reponse)))
        {
            emettre(reponse);
        }
        else
        {
            erreur(GF_INDEX);
        }
        break;
    case USB_TOUT:
        if (usb_service_espace_tx() < signaux_nombre() * USB_SERVICE_TRAME_SIGNAL_MAX_OCTETS)
        {
            erreur(GF_OCCUPE);
            break;
        }
        /* Propriete unique du modele : la suite est capturee avant de rendre la main. */
        for (size_t index_element = 0; index_element < signaux_nombre(); index_element++)
        {
            if (!usb_proto_formater_signal(index_element, reponse, sizeof(reponse)))
            {
                erreur(GF_ARGUMENT);
                break;
            }
            emettre(reponse);
        }
        break;
    case USB_ETAT:
        {
            const integration_statut_t *statut_systeme = integration_statut();
            unsigned a_modifie = 0, en_sauvegarde = 0;
            if (statut_systeme->modifie)
            {
                a_modifie = 1U;
            }
            if (statut_systeme->sauvegarde_en_cours)
            {
                en_sauvegarde = 1U;
            }
            snprintf(reponse, sizeof(reponse), "$STATE:%s:%d:%u:%" PRIu32 ":%" PRIu32 ":%u:%u:%s:%s:%s#", integration_nom_etat(), statut_systeme->selection + 1,
                (unsigned) signaux_nombre(), statut_systeme->revision, statut_systeme->revision_sauvee, a_modifie, en_sauvegarde, gf_result_nom(statut_systeme->sauvegarde),
                gf_result_nom(statut_systeme->erreur), gf_result_nom(statut_systeme->erreur_arret));
            emettre(reponse);
            break;
        }
    case USB_CHOISIR:
        ok(integration_choisir(commande->index), "SELECT");
        break;
    case USB_ON:
        ok(integration_activer(), "ON_COMMANDE");
        break;
    case USB_OFF:
        ok(integration_arreter(), "OFF_COMMANDE");
        break;
    case USB_SAUVER:
        ok(integration_sauver(), "SAVE_DEMANDE");
        break;
    case USB_CHARGER:
        ok(integration_charger(), "LOAD");
        break;
    case USB_USINE:
        ok(integration_usine(), "RAM");
        break;
    case USB_DUPLIQUER:
        ok(integration_dupliquer(commande->index, NULL), "RAM");
        break;
    case USB_SUPPRIMER:
        ok(integration_supprimer(commande->index), "RAM");
        break;
    case USB_DIGITAL:
        ok(integration_commander_digital((unsigned) commande->index, commande->frequence, commande->rapport, commande->actif), "DIGITAL");
        break;
    case USB_DIG_INFOS:
        for (unsigned canal = 0; canal < 2; canal++)
        {
            const digital_statut_t *statut_digital = integration_digital_statut(canal);
            unsigned disponible = 0, connu = 0, actif = 0;
            if (integration_sortie_digitale_disponible(canal))
            {
                disponible = 1U;
            }
            if (statut_digital->connu)
            {
                connu = 1U;
            }
            if (statut_digital->actif)
            {
                actif = 1U;
            }
            snprintf(reponse, sizeof(reponse), "$DIG:%u:%u:%u:%u:%" PRIu32 ":%u:%s#", canal + 1, disponible, connu, actif, statut_digital->frequence_hz,
                (unsigned) statut_digital->rapport_pm, gf_result_nom(statut_digital->erreur));
            emettre(reponse);
        }
        break;
    case USB_CAL_BEGIN:
        memset(image_calibration, 0, sizeof(image_calibration));
        masque_blocs_cal_recus = 0;
        calibration_en_cours = true;
        dernier_bloc_cal_ms = horloge_usb_ms;
        revision_calibration = integration_statut()->revision;
        ok(GF_OK, "CAL_BEGIN");
        break;
    case USB_CAL_DATA:
        if (!calibration_en_cours)
        {
            erreur(GF_CONFLIT);
            break;
        }
        memcpy(image_calibration + commande->position, commande->donnees, USB_CAL_BLOC_OCTETS);
        masque_blocs_cal_recus |=(uint16_t)(1U <<(commande->position / USB_CAL_BLOC_OCTETS));
        dernier_bloc_cal_ms = horloge_usb_ms;
        ok(GF_OK, "CAL_DATA");
        break;
    case USB_CAL_COMMIT:
        if (!calibration_en_cours || masque_blocs_cal_recus != UINT16_MAX || revision_calibration != integration_statut()->revision)
        {
            resultat = GF_CONFLIT;
        }
        else
        {
            resultat = integration_importer_calibration(image_calibration);
        }
        calibration_en_cours = false;
        ok(resultat, "RAM");
        break;
    case USB_CAL_ABORT:
        calibration_en_cours = false;
        ok(GF_OK, "CAL_ABORT");
        break;
    case USB_CAL_CLEAR:
        calibration_en_cours = false;
        ok(integration_effacer_calibration(), "RAM");
        break;
    case USB_CAL_GET:
        {
            if (calibration_en_cours)
            {
                erreur(GF_OCCUPE);
                break;
            }
            if (usb_service_espace_tx() < USB_SERVICE_CAL_RESERVE_TX_OCTETS)
            {
                erreur(GF_OCCUPE);
                break;
            }
            resultat = calibration_encoder(integration_calibration(), image_calibration);
            if (resultat != GF_OK)
            {
                erreur(resultat);
                break;
            }
            static const char hex[] = "0123456789ABCDEF";
            for (unsigned position_bloc = 0; position_bloc < CAL_IMAGE_SIZE; position_bloc += USB_CAL_BLOC_OCTETS)
            {
                int longueur_reponse = snprintf(reponse, sizeof(reponse), "$CAL:DATA:%u:", position_bloc);
                for (unsigned index_element = 0; index_element < USB_CAL_BLOC_OCTETS; index_element++)
                {
                    reponse[longueur_reponse++] = hex[image_calibration[position_bloc + index_element] >> 4];
                    reponse[longueur_reponse++] = hex[image_calibration[position_bloc + index_element] & 15];
                }
                reponse[longueur_reponse++] = '#';
                reponse[longueur_reponse] = 0;
                emettre(reponse);
            }
            emettre("$CAL:END#");
            break;
        }
    }
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : usb_service_octet()
 * Descriptif : Reconstruit une trame depuis les octets recus.
 *--------------------------------------------------------------------------------------------------*/
void usb_service_octet(uint8_t octet_recu, uint32_t maintenant_ms)
{
    horloge_usb_ms = maintenant_ms;
    if (octet_recu == '$')
    {
        buffer_rx[0] = '$';
        longueur_rx = 1;
        dernier_octet_rx_ms = maintenant_ms;
        return;
    }
    if (!longueur_rx)
    {
        return;
    }
    if (octet_recu < USB_SERVICE_CARACTERE_ASCII_MIN || octet_recu > USB_SERVICE_CARACTERE_ASCII_MAX)
    {
        longueur_rx = 0;
        stats.invalides++;
        erreur(GF_ARGUMENT);
        return;
    }
    if (longueur_rx >= sizeof(buffer_rx) - 1)
    {
        longueur_rx = 0;
        stats.depassements++;
        erreur(GF_CAPACITE);
        return;
    }
    buffer_rx[longueur_rx++] =(char) octet_recu;
    dernier_octet_rx_ms = maintenant_ms;
    if (octet_recu == '#')
    {
        buffer_rx[longueur_rx] = 0;
        usb_commande_t commande;
        gf_result_t resultat = usb_proto_decoder(buffer_rx, &commande);
        longueur_rx = 0;
        stats.trames++;
        if (resultat != GF_OK)
        {
            stats.invalides++;
            erreur(resultat);
        }
        else
        {
            executer(&commande);
        }
    }
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : usb_service_periodique()
 * Descriptif : Surveille les delais de reception et emission.
 *--------------------------------------------------------------------------------------------------*/
void usb_service_periodique(uint32_t maintenant_ms)
{
    horloge_usb_ms = maintenant_ms;
    if (longueur_rx &&(uint32_t)(maintenant_ms - dernier_octet_rx_ms) >= USB_SERVICE_TIMEOUT_RX_MS)
    {
        longueur_rx = 0;
        stats.abandons_rx++;
        erreur(GF_TIMEOUT);
    }
    if (calibration_en_cours &&(uint32_t)(maintenant_ms - dernier_bloc_cal_ms) >= USB_SERVICE_TIMEOUT_CAL_MS)
    {
        calibration_en_cours = false;
    }
    if (octets_a_emettre &&(uint32_t)(maintenant_ms - dernier_octet_tx_ms) >= USB_SERVICE_TIMEOUT_TX_MS)
    {
        octets_a_emettre = debut_tx = longueur_rx = 0;
        stats.abandons_tx++;
        reinitialiser_transport = true;
        /* Le transport vide aussi ses propres buffers pour eviter des queues stale. */
    }
}

const uint8_t *usb_service_tx(size_t *n)
{
    if (!n)
    {
        return NULL;
    }
    * n = octets_a_emettre;
    if (*n > USB_TX_CAPACITE - debut_tx)
    {
        *n = USB_TX_CAPACITE - debut_tx;
    }
    if (*n > USB_SERVICE_ENVOI_MAX_OCTETS)
    {
        *n = USB_SERVICE_ENVOI_MAX_OCTETS;
    }
    if (*n)
    {
        return buffer_tx + debut_tx;
    }
    return NULL;
}

void usb_service_tx_consomme(size_t nombre_octets, uint32_t maintenant_ms)
{
    if (nombre_octets > octets_a_emettre)
    {
        nombre_octets = octets_a_emettre;
    }
    debut_tx =(debut_tx + nombre_octets) % USB_TX_CAPACITE;
    octets_a_emettre -= nombre_octets;
    if (nombre_octets)
    {
        dernier_octet_tx_ms = maintenant_ms;
        stats.octets_tx +=(uint32_t) nombre_octets;
    }
}

bool usb_service_reinitialisation_transport(void)
{
    bool b = reinitialiser_transport;
    reinitialiser_transport = false;
    return b;
}

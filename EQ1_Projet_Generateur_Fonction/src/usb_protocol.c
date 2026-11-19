/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : USB_PROTOCOL.C
 * DESCRIPTIF : Decodeur et encodeur du protocole ASCII USB.
 *==================================================================================================*/
#include "usb_protocol.h"
#include "analogique.h"
#include "sorties_digitales.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/*============================== VARIABLES ET CONFIGURATION =======================================*/
#define USB_NOMBRE_CHAMPS_MAX           8U
#define USB_TRAME_LONGUEUR_MIN          3U
#define USB_ASCII_ESPACE_MAX            32U
#define USB_ASCII_IMPRIMABLE_MAX        126U
#define USB_INDEX_MIN                   1U
#define USB_HEX_BASE                   16
#define USB_HEX_CARACTERES_PAR_OCTET     2U
/*==================================== FONCTIONS ================================================*/
static bool nombre(const char *texte, int32_t minimum, int32_t maximum, bool signe_autorise, int32_t *valeur_destination)
{
    if (!texte || !* texte)
    {
        return false;
    }
    bool negatif = false;
    if (*texte == '-' || * texte == '+')
    {
        if (!signe_autorise)
        {
            return false;
        }
        negatif = *texte == '-';
        texte++;
    }
    if (!* texte)
    {
        return false;
    }
    uint64_t accumulateur = 0;
    for (; * texte; texte++)
    {
        if (*texte < '0' || * texte > '9')
        {
            return false;
        }
        accumulateur = accumulateur * 10 +(unsigned)(*texte - '0');
        if (accumulateur > ((uint64_t)INT32_MAX + 1U))
        {
            return false;
        }
    }
    int64_t valeur_signee =(int64_t) accumulateur;
    if (negatif)
    {
        valeur_signee = -(int64_t) accumulateur;
    }
    if (valeur_signee < minimum || valeur_signee > maximum)
    {
        return false;
    }
    * valeur_destination =(int32_t) valeur_signee;
    return true;
}

static int hex(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    return- 1;
}

/*================================ INTERFACE PUBLIQUE ============================================*/
/*--------------------------------------------------------------------------------------------------
 * Fonction   : usb_proto_decoder()
 * Descriptif : Valide une trame et extrait les champs de commande.
 *--------------------------------------------------------------------------------------------------*/
gf_result_t usb_proto_decoder(const char *trame, usb_commande_t *commande_destination)
{
    if (!trame || !commande_destination)
    {
        return GF_ARGUMENT;
    }
    size_t longueur_trame = 0;
    while (longueur_trame < USB_TRAME_MAX && trame[longueur_trame])
    {
        longueur_trame++;
    }
    if (longueur_trame < USB_TRAME_LONGUEUR_MIN || longueur_trame >= USB_TRAME_MAX || trame[0] != '$' || trame[longueur_trame - 1] != '#')
    {
        return GF_ARGUMENT;
    }
    char corps_trame[USB_TRAME_MAX];
    memcpy(corps_trame, trame + 1, longueur_trame - 2);
    corps_trame[longueur_trame - 2] = 0;
    for (size_t index_element = 0; index_element < longueur_trame - 2; index_element++)
    {
        if ((unsigned char) corps_trame[index_element] <= USB_ASCII_ESPACE_MAX ||(unsigned char) corps_trame[index_element] > USB_ASCII_IMPRIMABLE_MAX || corps_trame[index_element] == '$' || corps_trame[index_element] == '#')
        {
            return GF_ARGUMENT;
        }
    }
    usb_commande_t commande = { 0 };
    int32_t valeur_lue = 0;
    if (!strcmp(corps_trame, "?"))
    {
        commande.action = USB_TOUT;
        * commande_destination = commande;
        return GF_OK;
    }
    if (!strcmp(corps_trame, "STATE?"))
    {
        commande.action = USB_ETAT;
        * commande_destination = commande;
        return GF_OK;
    }
    if (!strcmp(corps_trame, "DIG?"))
    {
        commande.action = USB_DIG_INFOS;
        * commande_destination = commande;
        return GF_OK;
    }
    if (!strcmp(corps_trame, "CAL?"))
    {
        commande.action = USB_CAL_GET;
        * commande_destination = commande;
        return GF_OK;
    }
    if (corps_trame[longueur_trame - 3] == '?')
    {
        corps_trame[longueur_trame - 3] = 0;
        if (!nombre(corps_trame, USB_INDEX_MIN, SIGNAL_MAX, false, &valeur_lue))
        {
            return GF_ARGUMENT;
        }
        commande.action = USB_LIRE;
        commande.index =(size_t)(valeur_lue - 1);
        * commande_destination = commande;
        return GF_OK;
    }
    char *champs[USB_NOMBRE_CHAMPS_MAX];
    size_t nombre_champs = 1;
    champs[0] = corps_trame;
    for (char *curseur = corps_trame; * curseur; curseur++)
    {
        if (*curseur == ':')
        {
            if (nombre_champs == USB_NOMBRE_CHAMPS_MAX)
            {
                return GF_ARGUMENT;
            }
            * curseur = 0;
            champs[nombre_champs++] = curseur + 1;
        }
    }
    for (size_t index_element = 0; index_element < nombre_champs; index_element++)
    {
        if (!* champs[index_element])
        {
            return GF_ARGUMENT;
        }
    }
    if (nombre_champs == 1)
    {
        if (!strcmp(corps_trame, "ON"))
        {
            commande.action = USB_ON;
        }
        else if (!strcmp(corps_trame, "OFF"))
        {
            commande.action = USB_OFF;
        }
        else if (!strcmp(corps_trame, "SAVE"))
        {
            commande.action = USB_SAUVER;
        }
        else if (!strcmp(corps_trame, "LOAD"))
        {
            commande.action = USB_CHARGER;
        }
        else if (!strcmp(corps_trame, "FACTORY"))
        {
            commande.action = USB_USINE;
        }
        else
        {
            return GF_ARGUMENT;
        }
    }
    else if (nombre_champs == 2 &&(!strcmp(corps_trame, "SELECT") || !strcmp(corps_trame, "DELETE") || !strcmp(corps_trame, "DUP")))
    {
        if (!nombre(champs[1], USB_INDEX_MIN, SIGNAL_MAX, false, &valeur_lue))
        {
            return GF_ARGUMENT;
        }
        commande.index =(size_t)(valeur_lue - 1);
        if (!strcmp(corps_trame, "SELECT"))
        {
            commande.action = USB_CHOISIR;
        }
        else if (!strcmp(corps_trame, "DELETE"))
        {
            commande.action = USB_SUPPRIMER;
        }
        else
        {
            commande.action = USB_DUPLIQUER;
        }
    }
    else if (!strcmp(corps_trame, "CAL"))
    {
        if (nombre_champs == 2)
        {
            if (!strcmp(champs[1], "BEGIN"))
            {
                commande.action = USB_CAL_BEGIN;
            }
            else if (!strcmp(champs[1], "COMMIT"))
            {
                commande.action = USB_CAL_COMMIT;
            }
            else if (!strcmp(champs[1], "ABORT"))
            {
                commande.action = USB_CAL_ABORT;
            }
            else if (!strcmp(champs[1], "CLEAR"))
            {
                commande.action = USB_CAL_CLEAR;
            }
            else
            {
                return GF_ARGUMENT;
            }
        }
        else if (nombre_champs == 4 && !strcmp(champs[1], "DATA"))
        {
            if (!nombre(champs[2], 0, CAL_IMAGE_SIZE - USB_CAL_BLOC_OCTETS, false, &valeur_lue) || valeur_lue % USB_CAL_BLOC_OCTETS || strlen(champs[3]) != USB_CAL_HEX_CARACTERES)
            {
                return GF_ARGUMENT;
            }
            commande.action = USB_CAL_DATA;
            commande.position =(uint16_t) valeur_lue;
            for (unsigned index_element = 0; index_element < USB_CAL_BLOC_OCTETS; index_element++)
            {
                int chiffre_haut = hex(champs[3][USB_HEX_CARACTERES_PAR_OCTET * index_element]), chiffre_bas = hex(champs[3][USB_HEX_CARACTERES_PAR_OCTET * index_element + 1]);
                if (chiffre_haut < 0 || chiffre_bas < 0)
                {
                    return GF_ARGUMENT;
                }
                commande.donnees[index_element] =(uint8_t)(USB_HEX_BASE * chiffre_haut + chiffre_bas);
            }
        }
        else
        {
            return GF_ARGUMENT;
        }
    }
    else if (!strcmp(corps_trame, "DIG") &&(nombre_champs == 3 || nombre_champs == 4))
    {
        if (!nombre(champs[1], USB_INDEX_MIN, DIGITAL_NOMBRE_CANAUX, false, &valeur_lue))
        {
            return GF_ARGUMENT;
        }
        commande.index =(size_t)(valeur_lue - 1);
        commande.action = USB_DIGITAL;
        if (nombre_champs == 3)
        {
            if (strcmp(champs[2], "OFF"))
            {
                return GF_ARGUMENT;
            }
            commande.actif = false;
        }
        else
        {
            if (!nombre(champs[2], USB_INDEX_MIN, DIGITAL_FREQUENCE_MAX_HZ, false, &valeur_lue))
            {
                return GF_ARGUMENT;
            }
            commande.frequence =(uint32_t) valeur_lue;
            if (!nombre(champs[3], 0, DIGITAL_RAPPORT_MAX_PM, false, &valeur_lue))
            {
                return GF_ARGUMENT;
            }
            commande.rapport =(uint16_t) valeur_lue;
            commande.actif = true;
        }
    }
    else if (nombre_champs == 5)
    {
        if (!nombre(champs[0], USB_INDEX_MIN, SIGNAL_MAX, false, &valeur_lue))
        {
            return GF_ARGUMENT;
        }
        commande.action = USB_ECRIRE;
        commande.index =(size_t)(valeur_lue - 1);
        if (!strcmp(champs[1], "SIN"))
        {
            commande.signal.forme = SIGNAL_SINUS;
        }
        else if (!strcmp(champs[1], "TRI"))
        {
            commande.signal.forme = SIGNAL_TRIANGLE;
        }
        else if (!strcmp(champs[1], "CAR"))
        {
            commande.signal.forme = SIGNAL_CARRE;
        }
        else
        {
            return GF_ARGUMENT;
        }
        if (!nombre(champs[2], SIGNAL_AMPL_MIN_MVPP, SIGNAL_AMPL_MAX_MVPP, false, &commande.signal.amplitude_mvpp) || !nombre(champs[3], SIGNAL_OFFSET_MIN_MV,
            SIGNAL_OFFSET_MAX_MV, true, &commande.signal.offset_mv) || !nombre(champs[4], SIGNAL_FREQ_MIN_HZ, SIGNAL_FREQ_MAX_HZ, false, &valeur_lue))
        {
            return GF_ARGUMENT;
        }
        commande.signal.frequence_hz =(uint32_t) valeur_lue;
    }
    else
    {
        return GF_ARGUMENT;
    }
    * commande_destination = commande;
    return GF_OK;
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : usb_proto_formater()
 * Descriptif : Construit une trame ASCII de signal.
 *--------------------------------------------------------------------------------------------------*/
bool usb_proto_formater(size_t index, const signal_config_t *signal, char *destination, size_t taille_destination)
{
    if (!destination || !taille_destination)
    {
        return false;
    }
    destination[0] = 0;
    if (!signal || index >= SIGNAL_MAX || !signaux_valide(*signal))
    {
        return false;
    }
    int longueur_ecrite = snprintf(destination, taille_destination, "$%u:%s:%" PRId32 ":%+" PRId32 ":%" PRIu32 "#", (unsigned) index + 1, signaux_type_court(signal->forme),
        signal->amplitude_mvpp, signal->offset_mv, signal->frequence_hz);
    return longueur_ecrite >= 0 &&(size_t) longueur_ecrite < taille_destination;
}

bool usb_proto_formater_signal(size_t index, char *destination, size_t taille_destination)
{
    return usb_proto_formater(index, signaux_lire(index), destination, taille_destination);
}

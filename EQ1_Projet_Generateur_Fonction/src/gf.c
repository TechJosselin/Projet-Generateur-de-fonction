/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : GF.C
 * DESCRIPTIF : Codes de resultat communs et calcul CRC32.
 *==================================================================================================*/
#include "gf.h"

/*============================== VARIABLES ET CONFIGURATION =======================================*/
/* Polynome renverse IEEE 802.3 utilise pour le CRC-32. */
#define GF_CRC32_POLYNOME_RENVERSE UINT32_C(0xEDB88320)
#define GF_CRC32_BITS_PAR_OCTET    8U
/*==================================== FONCTIONS ================================================*/
/*================================ INTERFACE PUBLIQUE ============================================*/
const char *gf_result_nom(gf_result_t resultat)
{
    static const char *const names[] = {
        "OK",
        "ARGUMENT",
        "INDEX",
        "CAPACITE",
        "ABSENT",
        "IO",
        "TIMEOUT",
        "CRC",
        "VERSION",
        "VIDE",
        "NON_CALIBRE",
        "PLAGE",
        "CONFLIT",
        "OCCUPE",
        "VERIFICATION",
        "SELECTION",
        "NON_SUPPORTE"
    };
    if ((unsigned) resultat < sizeof(names) / sizeof(names[0]))
    {
        return names[resultat];
    }
    return "INCONNU";
}

uint32_t gf_crc32(const uint8_t *donnees, size_t longueur_donnees)
{
    uint32_t crc = UINT32_MAX;
    for (size_t index_octet = 0; index_octet < longueur_donnees; index_octet++)
    {
        crc ^= donnees[index_octet];
        for (unsigned index_bit = 0; index_bit < GF_CRC32_BITS_PAR_OCTET; index_bit++)
        {
            uint32_t poly = 0;
            if (crc & 1U)
            {
                poly = GF_CRC32_POLYNOME_RENVERSE;
            }
            crc =(crc >> 1) ^ poly;
        }
    }
    return ~crc;
}

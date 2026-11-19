/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : GF.C
 * DESCRIPTIF : Codes de resultat communs et calcul CRC32.
 *==================================================================================================*/
#include "gf.h"

/*============================== VARIABLES ET CONFIGURATION =======================================*/
/*==================================== FONCTIONS ================================================*/
/*================================ INTERFACE PUBLIQUE ============================================*/
const char *gf_result_nom(gf_result_t r)
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
    if ((unsigned) r < sizeof(names) / sizeof(names[0]))
    {
        return names[r];
    }
    return "INCONNU";
}

uint32_t gf_crc32(const uint8_t *data, size_t n)
{
    uint32_t c = UINT32_MAX;
    for (size_t i = 0; i < n; i++)
    {
        c ^= data[i];
        for (unsigned b = 0; b < 8; b++)
        {
            uint32_t poly = 0;
            if (c & 1U)
            {
                poly = UINT32_C(0xEDB88320);
            }
            c =(c >> 1) ^ poly;
        }
    }
    return ~c;
}

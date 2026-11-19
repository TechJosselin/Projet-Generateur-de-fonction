/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : ENTREE.C
 * DESCRIPTIF : Decodage de quadrature et anti-rebond du bouton.
 *==================================================================================================*/
#include "entree.h"

/*============================== VARIABLES ET CONFIGURATION =======================================*/
#define QUADRATURE_MASQUE_AB            0x03U
#define QUADRATURE_DECALAGE_ETAT        2U
#define QUADRATURE_TRANSITIONS_PAR_PAS   4
#define BOUTON_REBOND_MS                30U
#define BOUTON_APPUI_LONG_MS            800U
static const int8_t transitions[16] = {
    0,
    1,
    - 1,
    0,
    - 1,
    0,
    0,
    1,
    1,
    0,
    0,
    - 1,
    0,
    - 1,
    1,
    0
};
/*==================================== FONCTIONS ================================================*/
/*================================ INTERFACE PUBLIQUE ============================================*/
void quadrature_initialiser(quadrature_t *etat_quadrature, uint8_t etat_ab)
{
    etat_quadrature->precedent = etat_ab & QUADRATURE_MASQUE_AB;
    etat_quadrature->detente = etat_ab & QUADRATURE_MASQUE_AB;
    etat_quadrature->somme = 0;
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : quadrature_traiter()
 * Descriptif : Determine le sens de rotation avec la table des transitions.
 *--------------------------------------------------------------------------------------------------*/
int quadrature_traiter(quadrature_t *etat_quadrature, uint8_t etat_ab)
{
    etat_ab &= QUADRATURE_MASQUE_AB;
    if (etat_ab == etat_quadrature->precedent)
    {
        return 0;
    }
    int transition = transitions[(etat_quadrature->precedent << QUADRATURE_DECALAGE_ETAT) | etat_ab];
    if (transition)
    {
        etat_quadrature->somme += transition;
    }
    else
    {
        etat_quadrature->somme = 0;
    }
    etat_quadrature->precedent = etat_ab;
    if (etat_ab != etat_quadrature->detente)
    {
        return 0;
    }
    int sens_rotation = 0;
    if (etat_quadrature->somme >= QUADRATURE_TRANSITIONS_PAR_PAS)
    {
        sens_rotation = 1;
    }
    else if (etat_quadrature->somme <= -QUADRATURE_TRANSITIONS_PAR_PAS)
    {
        sens_rotation = -1;
    }
    etat_quadrature->somme = 0;
    return sens_rotation;
}

void bouton_initialiser(bouton_t *etat_bouton, bool appuye, uint32_t maintenant_ms)
{
    *etat_bouton =(bouton_t)
    {
        .brut = appuye,.stable = appuye,.rebond = maintenant_ms,.appui = maintenant_ms
    };
}

evenement_ui_t bouton_traiter(bouton_t *etat_bouton, bool appuye, uint32_t maintenant_ms)
{
    if (appuye != etat_bouton->brut)
    {
        etat_bouton->brut = appuye;
        etat_bouton->rebond = maintenant_ms;
    }
    if (etat_bouton->stable != appuye &&(uint32_t)(maintenant_ms - etat_bouton->rebond) >= BOUTON_REBOND_MS)
    {
        etat_bouton->stable = appuye;
        if (appuye)
        {
            etat_bouton->appui = maintenant_ms;
            etat_bouton->long_emis = false;
        }
        else if (!etat_bouton->long_emis)
        {
            return UI_CLIC;
        }
    }
    if (etat_bouton->stable && !etat_bouton->long_emis &&(uint32_t)(maintenant_ms - etat_bouton->appui) >= BOUTON_APPUI_LONG_MS)
    {
        etat_bouton->long_emis = true;
        return UI_APPUI_LONG;
    }
    return UI_AUCUN;
}

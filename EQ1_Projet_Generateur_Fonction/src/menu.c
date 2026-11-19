/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : MENU.C
 * DESCRIPTIF : Navigation OLED a trois lignes et choix des signaux.
 *==================================================================================================*/
/* Interface locale en lecture seule : parcourir, activer, arreter.
 * Les configurations et leur persistance sont gerees exclusivement via USB.
 * Rendu trois lignes conserve depuis feature/menu-oled-3-lignes. */
#include "menu.h"
#include "integration.h"
#include "oled.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/*============================== VARIABLES ET CONFIGURATION =======================================*/
/* Navigation sur trois lignes et defilement horizontal. */
#define MENU_LIGNES_VISIBLES             3
#define MENU_HAUTEUR_LIGNE_PX            17
#define MENU_ORIGINE_LIGNES_Y_PX         14
#define MENU_ATTENTE_DEFILEMENT_MS       900U
#define MENU_PAS_DEFILEMENT_MS           350U
#define MENU_PAUSE_DEFILEMENT_PAS        4U
#define MENU_RAFRAICHISSEMENT_MS         250U
#define MENU_TAMPON_TEXTE_OCTETS         64U
#define MENU_TAMPON_MESURE_OCTETS        24U
#define MENU_MARGE_X_PX                  4
#define MENU_SELECTION_HAUTEUR_PX       16
#define MENU_BANDEAU_Y_PX               10
#define MENU_MESSAGE_VIDE_LIGNE          3
#define MENU_FREQUENCE_Y_PX             14
#define MENU_AMPLITUDE_Y_PX             31
#define MENU_OFFSET_Y_PX                48
/* Types de pages de l'interface locale. */
typedef enum
{
    LISTE,      // Catalogue des signaux disponibles
    DETAIL,     // Parametres du signal selectionne
    MESSAGE     // Information ou erreur d'activation
} page_t;

/* Etat courant du menu : ces variables ne modifient pas le catalogue. */
static page_t page;                     // Page actuellement affichee
static int index_signal;                // Index pointe par l'encodeur
static int premiere_ligne;              // Premier element de la fenetre a trois lignes

/* Gestion du rafraichissement et du defilement de texte. */
static bool sale;                       // Un dessin de l'ecran est demande
static bool fenetre_valide;             // La fenetre visible a ete calculee
static bool action_en_attente;          // Une interaction vient d'etre recue
static bool defilement_necessaire;      // La ligne selectionnee depasse la largeur utile
static bool erreur_arret;              // Derniere erreur liee a une demande d'arret

/* Horodatages en millisecondes. */
static uint32_t horloge_menu_ms;                    // Horloge courante
static uint32_t dernier_dessin_ms;         // Date du dernier rafraichissement
static uint32_t dernier_evenement_ms;      // Date de la derniere action utilisateur
static uint32_t derniere_animation_ms;     // Date de l'animation precedente

static gf_result_t dernier_resultat;    // Code du dernier resultat a afficher
/*==================================== FONCTIONS ================================================*/
static int borne(int valeur, int minimum, int maximum)
{
    if (valeur < minimum)
    {
        return minimum;
    }
    if (valeur > maximum)
    {
        return maximum;
    }
    return valeur;
}

static void recadrer(void)
{
    size_t nombre_signaux = signaux_nombre();
    if (nombre_signaux)
    {
        index_signal = borne(index_signal, 0, (int) nombre_signaux - 1);
    }
    else
    {
        index_signal = 0;
    }
}

static void aller(page_t nouvelle_page)
{
    if (page != nouvelle_page)
    {
        fenetre_valide = false;
    }
    page = nouvelle_page;
    sale = true;
    action_en_attente = true;
    defilement_necessaire = false;
}

static void texte(int ligne, const char *format, ...)
{
    char buffer_texte[MENU_TAMPON_TEXTE_OCTETS];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(buffer_texte, sizeof(buffer_texte), format, arguments);
    va_end(arguments);
    oled_ligne(ligne, buffer_texte, false);
}

static void bandeau(const char *titre)
{
    oled_ligne(0, titre, true);
    oled_segment(0, MENU_BANDEAU_Y_PX, OLED_LARGEUR_PX - 1, MENU_BANDEAU_Y_PX);
}

static void ligne_signal(int index_signal_demande, char *destination, size_t taille_destination)
{
    const signal_config_t *signal = signaux_lire((size_t) index_signal_demande);
    if (!signal)
    {
        *destination = 0;
        return;
    }
    char amplitude_texte[MENU_TAMPON_MESURE_OCTETS], frequence_texte[MENU_TAMPON_MESURE_OCTETS];
    signaux_formater_tension(amplitude_texte, sizeof(amplitude_texte), signal->amplitude_mvpp, false);
    signaux_formater_frequence(frequence_texte, sizeof(frequence_texte), signal->frequence_hz);
    snprintf(destination, taille_destination, "%02d %s %s %s", index_signal_demande + 1, signaux_type_court(signal->forme), amplitude_texte, frequence_texte);
}

/* Bandeau fixe et trois grandes lignes ; seule la ligne pointee defile.
 * La rotation deplace le pointeur, jamais la configuration ni la sortie. */
/*--------------------------------------------------------------------------------------------------
 * Fonction   : liste()
 * Descriptif : Affiche trois lignes du catalogue et le defilement du choix.
 *--------------------------------------------------------------------------------------------------*/
static void liste(const char *titre)
{
    bandeau(titre);
    int nombre_signaux =(int) signaux_nombre();
    if (nombre_signaux <= 0)
    {
        texte(MENU_MESSAGE_VIDE_LIGNE, "AUCUN SIGNAL / USB");
        return;
    }
    int premiere_ligne_max = 0;
    if (nombre_signaux > MENU_LIGNES_VISIBLES)
    {
        premiere_ligne_max = nombre_signaux - MENU_LIGNES_VISIBLES;
    }
    if (!fenetre_valide)
    {
        premiere_ligne = borne(index_signal - 1, 0, premiere_ligne_max);
        fenetre_valide = true;
    }
    if (index_signal < premiere_ligne)
    {
        premiere_ligne = index_signal;
    }
    if (index_signal >= premiere_ligne + MENU_LIGNES_VISIBLES)
    {
        premiere_ligne = index_signal - (MENU_LIGNES_VISIBLES - 1);
    }
    premiere_ligne = borne(premiere_ligne, 0, premiere_ligne_max);
    for (int index_ligne = 0; index_ligne < MENU_LIGNES_VISIBLES && premiere_ligne + index_ligne < nombre_signaux; index_ligne++)
    {
        char texte_complet[MENU_TAMPON_TEXTE_OCTETS] = { 0 }, texte_visible[OLED_GRAND_NB_CARACTERES + 1];
        ligne_signal(premiere_ligne + index_ligne, texte_complet, sizeof(texte_complet));
        bool est_selectionne = premiere_ligne + index_ligne == index_signal;
        size_t longueur_texte = strlen(texte_complet), decalage_texte = 0;
        if (est_selectionne && longueur_texte > OLED_GRAND_NB_CARACTERES)
        {
            defilement_necessaire = true;
            uint32_t temps_depuis_evenement_ms = horloge_menu_ms - dernier_evenement_ms;
            if (temps_depuis_evenement_ms > MENU_ATTENTE_DEFILEMENT_MS)
            {
                size_t decalage_maximum = longueur_texte - OLED_GRAND_NB_CARACTERES;
                size_t phase_defilement =((temps_depuis_evenement_ms - MENU_ATTENTE_DEFILEMENT_MS) / MENU_PAS_DEFILEMENT_MS) %(decalage_maximum + MENU_PAUSE_DEFILEMENT_PAS);
                if (phase_defilement > decalage_maximum)
                {
                    decalage_texte = decalage_maximum;
                }
                else
                {
                    decalage_texte = phase_defilement;
                }
            }
        }
        size_t nombre_caracteres_visibles = longueur_texte - decalage_texte;
        if (nombre_caracteres_visibles > OLED_GRAND_NB_CARACTERES)
        {
            nombre_caracteres_visibles = OLED_GRAND_NB_CARACTERES;
        }
        memcpy(texte_visible, texte_complet + decalage_texte, nombre_caracteres_visibles);
        texte_visible[nombre_caracteres_visibles] = 0;
        if (!est_selectionne && longueur_texte > OLED_GRAND_NB_CARACTERES)
        {
            texte_visible[OLED_GRAND_NB_CARACTERES - 1] = '>';
        }
        int position_y_px = MENU_ORIGINE_LIGNES_Y_PX + index_ligne * MENU_HAUTEUR_LIGNE_PX;
        oled_texte_grand(MENU_MARGE_X_PX, position_y_px, texte_visible);
        if (est_selectionne)
        {
            oled_inverser_zone(0, position_y_px - 1, OLED_LARGEUR_PX, MENU_SELECTION_HAUTEUR_PX);
        }
    }
}

static void message(gf_result_t resultat, bool echec_arret)
{
    dernier_resultat = resultat;
    erreur_arret = echec_arret;
    aller(MESSAGE);
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : valider()
 * Descriptif : Selectionne un signal, puis n'active la sortie qu'en cas de succes.
 *--------------------------------------------------------------------------------------------------*/
static void valider(void)
{
    if (page != LISTE)
    {
        aller(LISTE);
        return;
    }
    /* Une seule validation locale : selection ET activation coordonnee.
     * Ne pas activer l'ancienne selection si choisir() a echoue. */
    gf_result_t resultat = integration_choisir((size_t) index_signal);
    if (resultat == GF_OK)
    {
        resultat = integration_activer();
    }
    if (resultat == GF_OK)
    {
        aller(DETAIL);
    }
    else
    {
        message(resultat, false);
    }
}

/*================================ INTERFACE PUBLIQUE ============================================*/
/*--------------------------------------------------------------------------------------------------
 * Fonction   : menu_initialiser()
 * Descriptif : Reinitialise la page, les indices et les temporisations.
 *--------------------------------------------------------------------------------------------------*/
void menu_initialiser(void)
{
    page = LISTE;
    index_signal = 0;
    premiere_ligne = 0;
    sale = true;
    horloge_menu_ms = 0;
    dernier_dessin_ms = 0;
    fenetre_valide = false;
    action_en_attente = true;
    defilement_necessaire = false;
    dernier_evenement_ms = 0;
    derniere_animation_ms = 0;
    erreur_arret = false;
    dernier_resultat = GF_OK;
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : menu_traiter()
 * Descriptif : Interprete les rotations, les clics et les appuis longs.
 *--------------------------------------------------------------------------------------------------*/
void menu_traiter(evenement_ui_t evenement)
{
    if (evenement != UI_ROTATION_PLUS && evenement != UI_ROTATION_MOINS && evenement != UI_CLIC && evenement != UI_APPUI_LONG)
    {
        return;
    }
    recadrer();
    action_en_attente = true;
    if (evenement == UI_APPUI_LONG)
    {
        gf_result_t resultat = integration_arreter();
        if (resultat == GF_OK)
        {
            aller(LISTE);
        }
        else
        {
            message(resultat, true);
        }
        return;
    }
    if (evenement == UI_CLIC)
    {
        valider();
        sale = true;
        return;
    }
    /* Depuis la fiche, tourner revient directement au choix suivant/precedent. */
    if (page != LISTE)
    {
        aller(LISTE);
    }
    int direction_rotation;
    if (evenement == UI_ROTATION_PLUS)
    {
        direction_rotation = 1;
    }
    else
    {
        direction_rotation = -1;
    }
    if (signaux_nombre())
    {
        index_signal = borne(index_signal + direction_rotation, 0, (int) signaux_nombre() - 1);
    }
    sale = true;
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : afficher()
 * Descriptif : Dessine l'ecran suivant l'etat reel du systeme.
 *--------------------------------------------------------------------------------------------------*/
static void afficher(void)
{
    const integration_statut_t *statut_systeme = integration_statut();
    recadrer();
    const signal_config_t *signal = signaux_lire((size_t) index_signal);
    char texte_affiche[MENU_TAMPON_TEXTE_OCTETS], valeur_texte[MENU_TAMPON_MESURE_OCTETS];
    oled_effacer();
    defilement_necessaire = false;
    bool sortie_active = statut_systeme->etat == SYS_ACTIF_COMMANDE;
    bool commande_nominale = sortie_active && statut_systeme->nominal_courant;
    bool dds_seul_actif = sortie_active && !statut_systeme->commande_courante && !commande_nominale;
    const char *label_liste = "ON";
    if (dds_seul_actif)
    {
        label_liste = "DDS";
    }
    else if (commande_nominale)
    {
        label_liste = "NOM";
    }
    const char *label_detail = "OFF";
    if (statut_systeme->etat == SYS_DEFAUT)
    {
        label_detail = "DEFAUT";
    }
    else if (sortie_active && statut_systeme->selection == index_signal)
    {
        if (dds_seul_actif)
        {
            label_detail = "DDS SEUL";
        }
        else if (commande_nominale)
        {
            label_detail = "NOM";
        }
        else
        {
            label_detail = "ON";
        }
    }
    const char *label_erreur = "SORTIE NON ACTIVEE";
    if (erreur_arret)
    {
        label_erreur = "ARRET NON CONFIRME";
    }
    switch (page)
    {
    case LISTE:
        if (statut_systeme->etat == SYS_DEFAUT)
        {
            snprintf(texte_affiche, sizeof(texte_affiche), "SIG %d/%u DEFAUT", index_signal + 1, (unsigned) signaux_nombre());
        }
        else if (sortie_active)
        {
            snprintf(texte_affiche, sizeof(texte_affiche), "SIG %d/%u %s S%d", index_signal + 1, (unsigned) signaux_nombre(), label_liste, statut_systeme->selection + 1);
        }
        else
        {
            snprintf(texte_affiche, sizeof(texte_affiche), "SIG %d/%u OFF", index_signal + 1, (unsigned) signaux_nombre());
        }
        liste(texte_affiche);
        break;
    case DETAIL:
        if (!signal)
        {
            aller(LISTE);
            liste("AUCUN SIGNAL / USB");
            break;
        }
        snprintf(texte_affiche, sizeof(texte_affiche), "S%d %s / %s", index_signal + 1, signaux_type_court(signal->forme), label_detail);
        bandeau(texte_affiche);
        signaux_formater_frequence(valeur_texte, sizeof(valeur_texte), signal->frequence_hz);
        snprintf(texte_affiche, sizeof(texte_affiche), "F %s", valeur_texte);
        oled_texte_grand(MENU_MARGE_X_PX, MENU_FREQUENCE_Y_PX, texte_affiche);
        if (dds_seul_actif && statut_systeme->selection == index_signal)
        {
            /* Les valeurs du catalogue ne sont PAS des tensions appliquees.
             * Ne pas afficher 1.000 Vpp comme si les EPOT avaient ete regles. */
            oled_texte_grand(MENU_MARGE_X_PX, MENU_AMPLITUDE_Y_PX, "A NON REGLEE");
            oled_texte_grand(MENU_MARGE_X_PX, MENU_OFFSET_Y_PX, "O NON REGLE");
        }
        else if (commande_nominale && statut_systeme->selection == index_signal)
        {
            /* ~ : estimation des codes commandes, pas mesure VOUT. */
            signaux_formater_tension(valeur_texte, sizeof(valeur_texte), statut_systeme->codes.amplitude_estimee_mvpp, false);
            snprintf(texte_affiche, sizeof(texte_affiche), "A~%spp", valeur_texte);
            oled_texte_grand(MENU_MARGE_X_PX, MENU_AMPLITUDE_Y_PX, texte_affiche);
            signaux_formater_tension(valeur_texte, sizeof(valeur_texte), statut_systeme->codes.offset_estime_mv, true);
            snprintf(texte_affiche, sizeof(texte_affiche), "O~%s", valeur_texte);
            oled_texte_grand(MENU_MARGE_X_PX, MENU_OFFSET_Y_PX, texte_affiche);
        }
        else
        {
            signaux_formater_tension(valeur_texte, sizeof(valeur_texte), signal->amplitude_mvpp, false);
            snprintf(texte_affiche, sizeof(texte_affiche), "A %spp", valeur_texte);
            oled_texte_grand(MENU_MARGE_X_PX, MENU_AMPLITUDE_Y_PX, texte_affiche);
            signaux_formater_tension(valeur_texte, sizeof(valeur_texte), signal->offset_mv, true);
            snprintf(texte_affiche, sizeof(texte_affiche), "O %s", valeur_texte);
            oled_texte_grand(MENU_MARGE_X_PX, MENU_OFFSET_Y_PX, texte_affiche);
        }
        break;
    case MESSAGE:
        texte(0, "%s", label_erreur);
        texte(2, "%s", gf_result_nom(dernier_resultat));
        texte(4, "%s", statut_systeme->etape);
        texte(6, "REGLAGES : USB SEUL");
        texte(7, "CLIC LISTE / LONG OFF");
        break;
    }
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : menu_periodique()
 * Descriptif : Planifie le rafraichissement et l'animation OLED.
 *--------------------------------------------------------------------------------------------------*/
void menu_periodique(uint32_t maintenant_ms)
{
    horloge_menu_ms = maintenant_ms;
    if (action_en_attente)
    {
        dernier_evenement_ms = horloge_menu_ms;
        derniere_animation_ms = horloge_menu_ms;
        action_en_attente = false;
    }
    /* Le defilement commence apres 900 ms, puis avance toutes les 350 ms. */
    if (defilement_necessaire &&(uint32_t)(horloge_menu_ms - dernier_evenement_ms) > MENU_ATTENTE_DEFILEMENT_MS &&(uint32_t)(horloge_menu_ms - derniere_animation_ms) >= MENU_PAS_DEFILEMENT_MS)
    {
        derniere_animation_ms = horloge_menu_ms;
        sale = true;
    }
    /* L'affichage est rafraichi sur modification ou au plus tard apres 250 ms. */
    if (sale ||(uint32_t)(horloge_menu_ms - dernier_dessin_ms) >= MENU_RAFRAICHISSEMENT_MS)
    {
        afficher();
        dernier_dessin_ms = horloge_menu_ms;
        sale = false;
    }
}

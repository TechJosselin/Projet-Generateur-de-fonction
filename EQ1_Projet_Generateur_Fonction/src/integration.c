/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : INTEGRATION.C
 * DESCRIPTIF : Coordination des etats, des sorties et de la sauvegarde.
 *==================================================================================================*/
#include "integration.h"
#include "sorties_digitales.h"
#include <string.h>

/*============================== VARIABLES ET CONFIGURATION =======================================*/
#define INTEGRATION_SAUVEGARDE_ATTENTE_MS    300U
static integration_backend_t materiel;
static stockage_t memoire;
static calibration_t profil;
static integration_statut_t etat;
static digital_statut_t digitales[DIGITAL_NOMBRE_CANAUX];
static stockage_donnees_t instantane;
/* Pas 2 ko de pile par sauvegarde. */
static uint32_t horloge, derniere_modification;
static bool sauvegarde_demandee;
/*==================================== FONCTIONS ================================================*/
/*================================ INTERFACE PUBLIQUE ============================================*/
const integration_statut_t *integration_statut(void)
{
    return&etat;
}

const calibration_t *integration_calibration(void)
{
    return&profil;
}

const digital_statut_t *integration_digital_statut(unsigned canal)
{
    if (canal < DIGITAL_NOMBRE_CANAUX)
    {
        return&digitales[canal];
    }
    return NULL;
}

const char *integration_nom_etat(void)
{
    static const char *const noms[] = {
        "NON_INITIALISE",
        "ARRET_COMMANDE",
        "PRET",
        "ACTIF_COMMANDE",
        "NON_CALIBRE",
        "DEFAUT"
    };
    if (etat.etat == SYS_ACTIF_COMMANDE && etat.nominal_courant)
    {
        return "NOMINAL_NON_CALIBRE";
    }
    if (etat.etat == SYS_ACTIF_COMMANDE && !etat.commande_courante)
    {
        return "DDS_SEUL_COMMANDE";
    }
    return noms[etat.etat];
}

bool integration_i2c_disponible(void)
{
    return materiel.i2c;
}

bool integration_ad9833_disponible(void)
{
    return materiel.dds;
}

bool integration_epot_disponible(void)
{
    return materiel.epots;
}

bool integration_memoire_disponible(void)
{
    return materiel.eeprom;
}

bool integration_sortie_digitale_disponible(unsigned canal)
{
    return canal < DIGITAL_NOMBRE_CANAUX && materiel.digitales[canal];
}

static gf_result_t resultat(gf_result_t code_operation, const char *etape)
{
    /* Le dernier defaut materiel reste visible jusqu'a un arret confirme. */
    if (etat.etat != SYS_DEFAUT)
    {
        etat.erreur = code_operation;
        etat.etape = etape;
    }
    return code_operation;
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : etat_arrete()
 * Descriptif : Deduit l'etat de repos selon le DDS et la calibration.
 *--------------------------------------------------------------------------------------------------*/
static void etat_arrete(void)
{
    if (!materiel.dds)
    {
        etat.etat = SYS_NON_INITIALISE;
    }
    else if (calibration_valider(&profil) != GF_OK)
    {
        etat.etat = SYS_NON_CALIBRE;
    }
    else if (etat.selection >= 0)
    {
        etat.etat = SYS_PRET;
    }
    else
    {
        etat.etat = SYS_ARRET_COMMANDE;
    }
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : changer()
 * Descriptif : Marque les modifications RAM et programme la sauvegarde.
 *--------------------------------------------------------------------------------------------------*/
static void changer(void)
{
    if (++ etat.revision == 0)
    {
        etat.revision = 1;
    }
    etat.modifie = true;
    sauvegarde_demandee = true;
    derniere_modification = horloge;
    if (materiel.eeprom)
    {
        etat.sauvegarde = GF_OK;
    }
    else
    {
        etat.sauvegarde = GF_ABSENT;
    }
}

static gf_result_t arreter_analogique(void)
{
    gf_result_t code_operation = GF_ABSENT;
    if (materiel.dds && materiel.arreter)
    {
        code_operation = materiel.arreter(materiel.ctx);
    }
    etat.erreur_arret = code_operation;
    etat.commande_courante = false;
    etat.nominal_courant = false;
    if (code_operation != GF_OK)
    {
        etat.erreur = code_operation;
        etat.etape = "ARRET_DDS";
        etat.etat = SYS_DEFAUT;
        return code_operation;
    }
    etat_arrete();
    return resultat(GF_OK, "ARRET_DDS");
}

static gf_result_t arreter_si_necessaire(void)
{
    /* Inclut le mode DDS seul, qui reste un DDS actif a arreter. */
    if (etat.etat == SYS_ACTIF_COMMANDE || etat.etat == SYS_DEFAUT)
    {
        return arreter_analogique();
    }
    return GF_OK;
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : integration_arreter()
 * Descriptif : Arrete toutes les sorties et signale les defauts.
 *--------------------------------------------------------------------------------------------------*/
gf_result_t integration_arreter(void)
{
    gf_result_t code_operation = arreter_analogique();
    for (unsigned canal = 0; canal < DIGITAL_NOMBRE_CANAUX; canal++)
    {
        if (materiel.digitales[canal])
        {
            gf_result_t resultat_digital = integration_commander_digital(canal, 0, 0, false);
            if (code_operation == GF_OK && resultat_digital != GF_OK)
            {
                code_operation = resultat_digital;
            }
        }
    }
    if (code_operation != GF_OK)
    {
        if (etat.etat != SYS_DEFAUT)
        {
            etat.erreur = code_operation;
            etat.etape = "ARRET";
        }
        etat.etat = SYS_DEFAUT;
        return code_operation;
    }
    return resultat(GF_OK, "ARRET");
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : integration_initialiser_avec()
 * Descriptif : Initialise les pilotes et charge l'EEPROM.
 *--------------------------------------------------------------------------------------------------*/
void integration_initialiser_avec(integration_backend_t backend, stockage_io_t io)
{
    materiel = backend;
    memset(&etat, 0, sizeof(etat));
    memset(&profil, 0, sizeof(profil));
    memset(digitales, 0, sizeof(digitales));
    etat.selection = -1;
    etat.revision = 1;
    horloge = 0;
    derniere_modification = 0;
    sauvegarde_demandee = false;
    signaux_initialiser();
    stockage_initialiser(&memoire, io);
    etat.chargement = GF_ABSENT;
    if (materiel.eeprom)
    {
        etat.chargement = stockage_charger(&memoire, &instantane);
    }
    etat.alerte_stockage = memoire.alerte;
    if (etat.chargement == GF_OK)
    {
        (void) signaux_remplacer_ensemble(instantane.signaux, instantane.nombre);
        profil = instantane.calibration;
        etat.revision_sauvee = etat.revision;
    }
    else
    {
        etat.modifie = true;
        /* Une memoire corrompue/version inconnue n'est jamais ecrasee au boot. */
        sauvegarde_demandee = etat.chargement == GF_VIDE;
    }
    etat.sauvegarde = GF_ABSENT;
    if (materiel.eeprom)
    {
        etat.sauvegarde = GF_OK;
    }
    etat.etape = "DEMARRAGE";
    etat_arrete();
    if (materiel.dds)
    {
        (void) arreter_analogique();
    }
    for (unsigned canal = 0; canal < DIGITAL_NOMBRE_CANAUX; canal++)
    {
        if (materiel.digitales[canal])
        {
            (void) integration_commander_digital(canal, 0, 0, false);
        }
    }
}

static void capturer(void)
{
    instantane.nombre = signaux_nombre();
    instantane.calibration = profil;
    for (size_t index_signal = 0; index_signal < instantane.nombre; index_signal++)
    {
        instantane.signaux[index_signal] = *signaux_lire(index_signal);
    }
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : integration_periodique()
 * Descriptif : Deroule la sauvegarde sans bloquer le programme.
 *--------------------------------------------------------------------------------------------------*/
void integration_periodique(uint32_t now)
{
    horloge = now;
    if (stockage_occupe(&memoire))
    {
        stockage_pas(&memoire, now);
        if (!stockage_occupe(&memoire))
        {
            etat.sauvegarde_en_cours = false;
            etat.sauvegarde = memoire.resultat;
            if (memoire.resultat == GF_OK)
            {
                etat.revision_sauvee = etat.revision_en_sauvegarde;
                if (etat.revision == etat.revision_sauvee)
                {
                    etat.modifie = false;
                    signaux_marquer_sauves();
                }
            }
        }
        return;
    }
    if (!etat.modifie)
    {
        sauvegarde_demandee = false;
    }
    if (sauvegarde_demandee &&(uint32_t)(now - derniere_modification) >= INTEGRATION_SAUVEGARDE_ATTENTE_MS)
    {
        sauvegarde_demandee = false;
        capturer();
        etat.sauvegarde = GF_ABSENT;
        if (materiel.eeprom)
        {
            etat.sauvegarde = stockage_commencer(&memoire, &instantane);
        }
        etat.sauvegarde_en_cours = etat.sauvegarde == GF_OK;
        if (etat.sauvegarde_en_cours)
        {
            etat.revision_en_sauvegarde = etat.revision;
        }
        /* Apres echec, seul un nouveau changement ou SAVE redemande une tentative. */
    }
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : integration_choisir()
 * Descriptif : Arrete si necessaire avant la selection d'un signal.
 *--------------------------------------------------------------------------------------------------*/
gf_result_t integration_choisir(size_t index)
{
    if (!signaux_lire(index))
    {
        return resultat(GF_INDEX, "SELECTION");
    }
    gf_result_t code_operation = arreter_si_necessaire();
    if (code_operation != GF_OK)
    {
        return code_operation;
    }
    etat.selection =(int) index;
    etat.commande_courante = false;
    etat.nominal_courant = false;
    etat_arrete();
    return resultat(GF_OK, "SELECTION_RAM");
}

static gf_result_t defaut(gf_result_t code_operation, const char *etape)
{
    etat.erreur_arret = GF_ABSENT;
    if (materiel.arreter)
    {
        etat.erreur_arret = materiel.arreter(materiel.ctx);
    }
    etat.erreur = code_operation;
    etat.etape = etape;
    etat.etat = SYS_DEFAUT;
    etat.commande_courante = false;
    etat.nominal_courant = false;
    return code_operation;
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : integration_activer()
 * Descriptif : Programme le DDS, le gain et l'offset avant activation.
 *--------------------------------------------------------------------------------------------------*/
gf_result_t integration_activer(void)
{
    if (etat.etat == SYS_DEFAUT)
    {
        return resultat(GF_CONFLIT, "ARRET_REQUIS");
    }
    const signal_config_t *signal = NULL;
    if (etat.selection >= 0)
    {
        signal = signaux_lire((size_t) etat.selection);
    }
    if (!signal)
    {
        return resultat(GF_SELECTION, "SELECTION");
    }
    analogique_commande_t codes = { 0 };
    gf_result_t code_operation = analogique_convertir(&profil, signal, &codes);
    bool sans_profil = code_operation == GF_NON_CALIBRE;
    if (code_operation != GF_OK && !sans_profil)
    {
        return resultat(code_operation, "CALIBRATION");
    }
    /* Profil approuve prioritaire. En son absence, EPOT nominaux si disponibles,
    * sinon DDS seul explicitement indique. Aucune mesure n'est simulee. */
    bool epot_ok = materiel.epots && materiel.gain && materiel.offset;
    bool nominal = sans_profil && epot_ok;
    bool dds_seul = sans_profil && !epot_ok;
    if (nominal)
    {
        code_operation = analogique_convertir_nominal(signal, &codes);
        if (code_operation != GF_OK)
        {
            return resultat(code_operation, "MODELE_NOMINAL");
        }
    }
    if (!materiel.dds || !materiel.arreter || !materiel.dds_configurer || !materiel.activer)
    {
        return resultat(GF_ABSENT, "MATERIEL_DDS");
    }
    if (!sans_profil && !epot_ok)
    {
        return resultat(GF_ABSENT, "MATERIEL_EPOT");
    }
    if (!sans_profil && profil.stabilisation_ms && !materiel.attendre_ms)
    {
        return resultat(GF_NON_SUPPORTE, "STABILISATION");
    }
    code_operation = arreter_analogique();
    if (code_operation != GF_OK)
    {
        return code_operation;
    }
    code_operation = materiel.dds_configurer(materiel.ctx, signal);
    if (code_operation != GF_OK)
    {
        return defaut(code_operation, "DDS_CONFIG");
    }
    if (!dds_seul)
    {
        if (nominal || profil.gain_avant_offset)
        {
            code_operation = materiel.gain(materiel.ctx, codes.gain);
            if (code_operation != GF_OK)
            {
                return defaut(code_operation, "GAIN");
            }
            code_operation = materiel.offset(materiel.ctx, codes.offset);
            if (code_operation != GF_OK)
            {
                return defaut(code_operation, "OFFSET");
            }
        }
        else
        {
            code_operation = materiel.offset(materiel.ctx, codes.offset);
            if (code_operation != GF_OK)
            {
                return defaut(code_operation, "OFFSET");
            }
            code_operation = materiel.gain(materiel.ctx, codes.gain);
            if (code_operation != GF_OK)
            {
                return defaut(code_operation, "GAIN");
            }
        }
        if (!sans_profil && profil.stabilisation_ms)
        {
            materiel.attendre_ms(materiel.ctx, profil.stabilisation_ms);
        }
    }
    code_operation = materiel.activer(materiel.ctx);
    if (code_operation != GF_OK)
    {
        return defaut(code_operation, "ACTIVATION");
    }
    etat.etat = SYS_ACTIF_COMMANDE;
    etat.nominal_courant = nominal;
    if (dds_seul)
    {
        etat.commande_courante = false;
        return resultat(GF_OK, "DDS_SEUL_NON_CALIBRE");
    }
    etat.codes = codes;
    if (nominal)
    {
        etat.commande_courante = false;
        /* Derniere = commande calibree historique. */
        return resultat(GF_OK, "EPOT_NOMINAL_NON_MESURE");
    }
    etat.derniere = *signal;
    etat.derniere_valide = true;
    etat.commande_courante = true;
    return resultat(GF_OK, "COMMANDE_COMPLETE");
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : integration_modifier()
 * Descriptif : Valide et applique une edition de signal en RAM.
 *--------------------------------------------------------------------------------------------------*/
gf_result_t integration_modifier(size_t index_signal, signal_config_t signal, uint32_t attendu)
{
    if (attendu && attendu != etat.revision)
    {
        return resultat(GF_CONFLIT, "EDITION");
    }
    if (!signaux_valide(signal))
    {
        return resultat(GF_ARGUMENT, "EDITION");
    }
    if (index_signal > signaux_nombre() || index_signal >= SIGNAL_MAX)
    {
        return resultat(GF_INDEX, "EDITION");
    }
    const signal_config_t *signal_existant = signaux_lire(index_signal);
    if (signal_existant && signaux_egaux(*signal_existant, signal))
    {
        return resultat(GF_OK, "INCHANGE");
    }
    if ((int) index_signal == etat.selection)
    {
        gf_result_t code_operation = arreter_si_necessaire();
        if (code_operation != GF_OK)
        {
            return code_operation;
        }
        etat.commande_courante = false;
    }
    bool ok;
    if (signal_existant)
    {
        ok = signaux_modifier(index_signal, signal);
    }
    else
    {
        ok = signaux_ajouter(signal);
    }
    if (!ok)
    {
        return resultat(GF_CAPACITE, "EDITION");
    }
    changer();
    return resultat(GF_OK, "RAM");
}

gf_result_t integration_dupliquer(size_t index_signal, int *nouveau)
{
    int nouvel_index = signaux_dupliquer(index_signal);
    if (nouvel_index < 0)
    {
        return resultat(GF_CAPACITE, "DUPLIQUER");
    }
    if (nouveau)
    {
        *nouveau = nouvel_index;
    }
    changer();
    return resultat(GF_OK, "RAM");
}

gf_result_t integration_supprimer(size_t index_signal)
{
    if (!signaux_lire(index_signal))
    {
        return resultat(GF_INDEX, "SUPPRIMER");
    }
    if (signaux_nombre() <= 1)
    {
        return resultat(GF_CAPACITE, "SUPPRIMER");
    }
    gf_result_t code_operation = arreter_si_necessaire();
    if (code_operation != GF_OK)
    {
        return code_operation;
    }
    (void) signaux_supprimer(index_signal);
    etat.selection = -1;
    etat.commande_courante = false;
    etat_arrete();
    changer();
    return resultat(GF_OK, "RAM");
}

gf_result_t integration_usine(void)
{
    gf_result_t code_operation = arreter_si_necessaire();
    if (code_operation != GF_OK)
    {
        return code_operation;
    }
    signaux_retablir_usine();
    etat.selection = -1;
    etat.commande_courante = false;
    etat_arrete();
    changer();
    return resultat(GF_OK, "RAM");
}

gf_result_t integration_sauver(void)
{
    if (!materiel.eeprom)
    {
        return resultat(GF_ABSENT, "SAUVEGARDE");
    }
    if (etat.modifie)
    {
        sauvegarde_demandee = true;
        derniere_modification = horloge - INTEGRATION_SAUVEGARDE_ATTENTE_MS;
    }
    return resultat(GF_OK, "SAUVEGARDE_DEMANDEE");
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : integration_charger()
 * Descriptif : Recharge la configuration persistante.
 *--------------------------------------------------------------------------------------------------*/
gf_result_t integration_charger(void)
{
    if (stockage_occupe(&memoire))
    {
        return resultat(GF_OCCUPE, "CHARGER");
    }
    if (!materiel.eeprom)
    {
        return resultat(GF_ABSENT, "CHARGER");
    }
    gf_result_t code_operation = arreter_si_necessaire();
    if (code_operation != GF_OK)
    {
        return code_operation;
    }
    code_operation = stockage_charger(&memoire, &instantane);
    etat.chargement = code_operation;
    etat.alerte_stockage = memoire.alerte;
    if (code_operation != GF_OK)
    {
        return resultat(code_operation, "CHARGER");
    }
    (void) signaux_remplacer_ensemble(instantane.signaux, instantane.nombre);
    profil = instantane.calibration;
    changer();
    sauvegarde_demandee = false;
    etat.modifie = false;
    etat.revision_sauvee = etat.revision;
    etat.selection = -1;
    etat.commande_courante = false;
    etat_arrete();
    return resultat(GF_OK, "CHARGE");
}

gf_result_t integration_importer_calibration(const uint8_t image_binaire[CAL_IMAGE_SIZE])
{
    calibration_t profil_decode;
    gf_result_t code_operation = calibration_decoder(image_binaire, &profil_decode);
    if (code_operation != GF_OK)
    {
        return resultat(code_operation, "PROFIL");
    }
    code_operation = calibration_valider(&profil_decode);
    if (code_operation != GF_OK)
    {
        return resultat(code_operation, "PROFIL");
    }
    code_operation = arreter_si_necessaire();
    if (code_operation != GF_OK)
    {
        return code_operation;
    }
    profil = profil_decode;
    etat.commande_courante = false;
    etat_arrete();
    changer();
    return resultat(GF_OK, "PROFIL_RAM");
}

gf_result_t integration_effacer_calibration(void)
{
    gf_result_t code_operation = arreter_si_necessaire();
    if (code_operation != GF_OK)
    {
        return code_operation;
    }
    memset(&profil, 0, sizeof(profil));
    etat.commande_courante = false;
    etat_arrete();
    changer();
    return resultat(GF_OK, "NON_CALIBRE");
}

gf_result_t integration_commander_digital(unsigned canal, uint32_t frequence_hz, uint16_t rapport_pm, bool actif)
{
    if (canal >= DIGITAL_NOMBRE_CANAUX || rapport_pm > DIGITAL_RAPPORT_MAX_PM ||(actif &&(!frequence_hz || frequence_hz > DIGITAL_FREQUENCE_MAX_HZ)))
    {
        return resultat(GF_ARGUMENT, "DIGITAL");
    }
    if (!materiel.digitales[canal] || !materiel.digital)
    {
        return resultat(GF_NON_SUPPORTE, "DIGITAL");
    }
    gf_result_t code_operation = materiel.digital(materiel.ctx, canal, frequence_hz, rapport_pm, actif);
    digitales[canal].erreur = code_operation;
    digitales[canal].connu = code_operation == GF_OK;
    if (code_operation == GF_OK)
    {
        digitales[canal].actif = actif;
        digitales[canal].frequence_hz = frequence_hz;
        digitales[canal].rapport_pm = rapport_pm;
    }
    return resultat(code_operation, "DIGITAL");
}

/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : EEPROM.C
 * DESCRIPTIF : Communication I2C et acces a la memoire 24LC32.
 *==================================================================================================*/
/* 24LC32A : adresse memoire 16 bits, 4096 octets, pages de 32 octets.
* Le cycle d'ecriture est suivi par ACK polling dans stockage_pas(), sans spin. */
#include "eeprom.h"
#include "stockage.h"
#include <string.h>

/*============================== VARIABLES ET CONFIGURATION =======================================*/
#define EEPROM_ADRESSE_I2C_MIN            0x50U
#define EEPROM_ADRESSE_I2C_MAX            0x57U
#define EEPROM_VITESSE_I2C_HZ             100000U
#define EEPROM_TIMEOUT_PROBE_MS           30
#define EEPROM_TIMEOUT_REQUETE_MS         30
#define EEPROM_TIMEOUT_CYCLE_MS           20
#define EEPROM_TAILLE_ADRESSE_OCTETS      2U
#define EEPROM_LECTURE_BLOC_MAX_OCTETS    128U
#define EEPROM_TRAME_ECRITURE_MAX_OCTETS  (STOCKAGE_PAGE + EEPROM_TAILLE_ADRESSE_OCTETS)
static i2c_master_bus_handle_t bus_eeprom;
static i2c_master_dev_handle_t dispositif_eeprom;
static uint8_t adresse_i2c_eeprom;
/*==================================== FONCTIONS ================================================*/
static gf_result_t convertir(esp_err_t erreur_i2c)
{
    if (erreur_i2c == ESP_OK)
    {
        return GF_OK;
    }
    if (erreur_i2c == ESP_ERR_TIMEOUT)
    {
        return GF_TIMEOUT;
    }
    return GF_IO;
}

/*================================ INTERFACE PUBLIQUE ============================================*/
/*--------------------------------------------------------------------------------------------------
 * Fonction   : eeprom_initialiser()
 * Descriptif : Verifie l'adresse de la memoire I2C.
 *--------------------------------------------------------------------------------------------------*/
esp_err_t eeprom_initialiser(i2c_master_bus_handle_t bus_i2c, uint8_t adresse_cible)
{
    if (!bus_i2c || adresse_cible < EEPROM_ADRESSE_I2C_MIN || adresse_cible > EEPROM_ADRESSE_I2C_MAX)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (dispositif_eeprom)
    {
        if (bus_i2c == bus_eeprom && adresse_cible == adresse_i2c_eeprom)
        {
            return ESP_OK;
        }
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t erreur_i2c = i2c_master_probe(bus_i2c, adresse_cible, EEPROM_TIMEOUT_PROBE_MS);
    if (erreur_i2c != ESP_OK)
    {
        return erreur_i2c;
    }
    i2c_device_config_t configuration_i2c = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = adresse_cible,
        .scl_speed_hz = EEPROM_VITESSE_I2C_HZ
    };
    erreur_i2c = i2c_master_bus_add_device(bus_i2c, &configuration_i2c, &dispositif_eeprom);
    if (erreur_i2c == ESP_OK)
    {
        bus_eeprom = bus_i2c;
        adresse_i2c_eeprom = adresse_cible;
    }
    return erreur_i2c;
}

static gf_result_t lecture(void *contexte, uint16_t adresse_cible, uint8_t *destination, size_t longueur)
{
    (void) contexte;
    if (!dispositif_eeprom)
    {
        return GF_ABSENT;
    }
    if (!destination || adresse_cible > STOCKAGE_TAILLE || longueur >(size_t)(STOCKAGE_TAILLE - adresse_cible))
    {
        return GF_ARGUMENT;
    }
    while (longueur)
    {
        size_t taille_bloc = longueur;
        if (longueur > EEPROM_LECTURE_BLOC_MAX_OCTETS)
        {
            taille_bloc = EEPROM_LECTURE_BLOC_MAX_OCTETS;
        }
        uint8_t octets_adresse[2] = {
            (uint8_t)(adresse_cible >> 8),
            (uint8_t) adresse_cible
        };
        esp_err_t erreur_i2c = i2c_master_transmit_receive(dispositif_eeprom, octets_adresse, EEPROM_TAILLE_ADRESSE_OCTETS, destination, taille_bloc, EEPROM_TIMEOUT_REQUETE_MS);
        if (erreur_i2c != ESP_OK)
        {
            return convertir(erreur_i2c);
        }
        adresse_cible =(uint16_t)(adresse_cible + taille_bloc);
        destination += taille_bloc;
        longueur -= taille_bloc;
    }
    return GF_OK;
}

static gf_result_t ecriture_page(void *contexte, uint16_t adresse_cible, const uint8_t *source, size_t longueur)
{
    (void) contexte;
    if (!dispositif_eeprom)
    {
        return GF_ABSENT;
    }
    if (!source || !longueur || longueur > STOCKAGE_PAGE || adresse_cible >= STOCKAGE_TAILLE || longueur >(size_t)(STOCKAGE_TAILLE - adresse_cible) || adresse_cible / STOCKAGE_PAGE !=(adresse_cible + longueur - 1) / STOCKAGE_PAGE)
    {
        return GF_ARGUMENT;
    }
    uint8_t trame[EEPROM_TRAME_ECRITURE_MAX_OCTETS] = {
        (uint8_t)(adresse_cible >> 8),
        (uint8_t) adresse_cible
    };
    memcpy(trame + EEPROM_TAILLE_ADRESSE_OCTETS, source, longueur);
    return convertir(i2c_master_transmit(dispositif_eeprom, trame, longueur + EEPROM_TAILLE_ADRESSE_OCTETS, EEPROM_TIMEOUT_REQUETE_MS));
}

static gf_result_t pret(void *contexte)
{
    (void) contexte;
    if (!dispositif_eeprom)
    {
        return GF_ABSENT;
    }
    esp_err_t erreur_i2c = i2c_master_probe(bus_eeprom, adresse_i2c_eeprom, EEPROM_TIMEOUT_CYCLE_MS);
    if (erreur_i2c == ESP_ERR_NOT_FOUND)
    {
        return GF_OCCUPE;
    }
    return convertir(erreur_i2c);
}

stockage_io_t eeprom_interface(void)
{
    return(stockage_io_t)
    {
        NULL, lecture, ecriture_page, pret
    };
}

/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : EPOT.C
 * DESCRIPTIF : Commande et verification des potentiometres MCP45HV51.
 *==================================================================================================*/
/* CFPT - MCP45HV51. Protocoles conserves depuis le pilote du projet.
* DS20005304B sections 4.4.1.2 et 7.5 : lecture deux octets, donnee dans le LSB.
* WLAT doit autoriser le transfert et SHDN etre inactif : a verifier sur la carte. */
#include "epot.h"
#include <stddef.h>

/*============================== VARIABLES ET CONFIGURATION =======================================*/
/* Champs du protocole MCP45HV51 : codes TCON et WIPER. */
#define EPOT_NOMBRE_CANAUX               2U
#define EPOT_ADRESSE_I2C_MIN             0x3CU
#define EPOT_ADRESSE_I2C_MAX             0x3FU
#define EPOT_TAILLE_REPONSE_LECTURE      2U
#define EPOT_REGISTRE_DECALAGE_BITS      4U
#define EPOT_COMMANDE_LECTURE_BITS       0x0CU
#define EPOT_REGISTRE_WIPER              0U
#define EPOT_REGISTRE_TCON               4U
#define EPOT_TCON_TOUT_ACTIF             0xFFU
static struct
{
    i2c_master_dev_handle_t handle;
    uint8_t dernier_code;
    bool valide;
}

potentiometres[EPOT_NOMBRE_CANAUX];
static bool initialise;
static i2c_master_bus_handle_t bus_initial;
static uint8_t adresses[EPOT_NOMBRE_CANAUX];
/*==================================== FONCTIONS ================================================*/
static bool canal_valide(epot_channel_t canal)
{
    return(unsigned) canal < EPOT_NOMBRE_CANAUX;
}

static esp_err_t lire(i2c_master_dev_handle_t dispositif, uint8_t registre, uint8_t *code)
{
    uint8_t commande =(uint8_t)((registre << EPOT_REGISTRE_DECALAGE_BITS) | EPOT_COMMANDE_LECTURE_BITS), reponse[EPOT_TAILLE_REPONSE_LECTURE];
    esp_err_t resultat_i2c = i2c_master_transmit_receive(dispositif, &commande, 1, reponse, EPOT_TAILLE_REPONSE_LECTURE, EPOT_I2C_TIMEOUT_MS);
    if (resultat_i2c != ESP_OK)
    {
        return resultat_i2c;
    }
    if (reponse[0] != 0)
    {
        return ESP_ERR_INVALID_RESPONSE;
    }
    * code = reponse[1];
    return ESP_OK;
}

static esp_err_t ecrire_verifier(i2c_master_dev_handle_t dispositif, uint8_t registre, uint8_t code)
{
    uint8_t trame[EPOT_TAILLE_REPONSE_LECTURE] = {
        (uint8_t)(registre << EPOT_REGISTRE_DECALAGE_BITS),
        code
    }, code_relu = 0;
    esp_err_t resultat_i2c = i2c_master_transmit(dispositif, trame, sizeof(trame), EPOT_I2C_TIMEOUT_MS);
    if (resultat_i2c != ESP_OK)
    {
        return resultat_i2c;
    }
    resultat_i2c = lire(dispositif, registre, &code_relu);
    if (resultat_i2c != ESP_OK)
    {
        return resultat_i2c;
    }
    if (code_relu == code)
    {
        return ESP_OK;
    }
    return ESP_ERR_INVALID_RESPONSE;
}

/*================================ INTERFACE PUBLIQUE ============================================*/
esp_err_t Epot_Deinit(void)
{
    esp_err_t premier = ESP_OK;
    initialise = false;
    for (unsigned canal = 0; canal < EPOT_NOMBRE_CANAUX; canal++)
    {
        potentiometres[canal].valide = false;
        if (potentiometres[canal].handle)
        {
            esp_err_t resultat_i2c = i2c_master_bus_rm_device(potentiometres[canal].handle);
            if (resultat_i2c == ESP_OK)
            {
                potentiometres[canal].handle = NULL;
            }
            else if (premier == ESP_OK)
            {
                premier = resultat_i2c;
            }
        }
    }
    return premier;
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : Epot_Init()
 * Descriptif : Associe les deux adresses I2C aux fonctions gain et offset.
 *--------------------------------------------------------------------------------------------------*/
esp_err_t Epot_Init(i2c_master_bus_handle_t bus, uint8_t adresse_gain, uint8_t adresse_offset)
{
    if (!bus || adresse_gain < EPOT_ADRESSE_I2C_MIN || adresse_gain > EPOT_ADRESSE_I2C_MAX || adresse_offset < EPOT_ADRESSE_I2C_MIN || adresse_offset > EPOT_ADRESSE_I2C_MAX || adresse_gain == adresse_offset)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (initialise)
    {
        if (bus == bus_initial && adresse_gain == adresses[0] && adresse_offset == adresses[1])
        {
            return ESP_OK;
        }
        return ESP_ERR_INVALID_STATE;
    }
    if (potentiometres[0].handle || potentiometres[1].handle)
    {
        esp_err_t resultat_i2c = Epot_Deinit();
        if (resultat_i2c != ESP_OK)
        {
            return resultat_i2c;
        }
    }
    const uint8_t adresses_a_tester[EPOT_NOMBRE_CANAUX] = { adresse_gain, adresse_offset };
    esp_err_t resultat_i2c;
    for (unsigned canal = 0; canal < EPOT_NOMBRE_CANAUX; canal++)
    {
        resultat_i2c = i2c_master_probe(bus, adresses_a_tester[canal], EPOT_I2C_TIMEOUT_MS);
        if (resultat_i2c != ESP_OK)
        {
            goto fail;
        }
        i2c_device_config_t configuration_i2c = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = adresses_a_tester[canal],
            .scl_speed_hz = EPOT_I2C_SPEED_HZ
        };
        resultat_i2c = i2c_master_bus_add_device(bus, &configuration_i2c, &potentiometres[canal].handle);
        if (resultat_i2c != ESP_OK)
        {
            goto fail;
        }
        resultat_i2c = ecrire_verifier(potentiometres[canal].handle, EPOT_REGISTRE_TCON, EPOT_TCON_TOUT_ACTIF);
        if (resultat_i2c != ESP_OK)
        {
            goto fail;
        }
        potentiometres[canal].valide = false;
    }
    bus_initial = bus;
    adresses[0] = adresse_gain;
    adresses[1] = adresse_offset;
    initialise = true;
    return ESP_OK;
    fail :(void) Epot_Deinit();
    return resultat_i2c;
}

esp_err_t Epot_ReadRaw(epot_channel_t canal, uint8_t *code)
{
    if (!canal_valide(canal) || !code)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (!initialise || !potentiometres[canal].handle)
    {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t resultat_i2c = lire(potentiometres[canal].handle, EPOT_REGISTRE_WIPER, code);
    if (resultat_i2c != ESP_OK)
    {
        potentiometres[canal].valide = false;
    }
    return resultat_i2c;
}

esp_err_t Epot_SetRaw(epot_channel_t canal, uint8_t code)
{
    if (!canal_valide(canal))
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (!initialise || !potentiometres[canal].handle)
    {
        return ESP_ERR_INVALID_STATE;
    }
    potentiometres[canal].valide = false;
    esp_err_t resultat_i2c = ecrire_verifier(potentiometres[canal].handle, EPOT_REGISTRE_WIPER, code);
    if (resultat_i2c == ESP_OK)
    {
        potentiometres[canal].dernier_code = code;
        potentiometres[canal].valide = true;
    }
    return resultat_i2c;
}

esp_err_t Epot_SetGainRaw(uint8_t code)
{
    return Epot_SetRaw(EPOT_CHANNEL_GAIN, code);
}

esp_err_t Epot_SetOffsetRaw(uint8_t code)
{
    return Epot_SetRaw(EPOT_CHANNEL_OFFSET, code);
}

esp_err_t Epot_GetLastRaw(epot_channel_t canal, uint8_t *code)
{
    if (!canal_valide(canal) || !code)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (!initialise || !potentiometres[canal].valide)
    {
        return ESP_ERR_INVALID_STATE;
    }
    * code = potentiometres[canal].dernier_code;
    return ESP_OK;
}

bool Epot_IsInitialized(void)
{
    return initialise;
}

bool Epot_IsChannelAvailable(epot_channel_t canal)
{
    return initialise && canal_valide(canal) && potentiometres[canal].handle;
}

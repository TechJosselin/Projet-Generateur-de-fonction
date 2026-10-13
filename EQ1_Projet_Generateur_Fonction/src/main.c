/* Generateur de fonctions CFPT - integration interface V4.
 * L'encodeur pilote uniquement le menu. La selection d'un preset prepare l'AD9833.
 * Les EPOT restent disponibles comme drivers mais ne sont pas commandes tant que
 * la conversion amplitude/offset n'est pas calibree.
 */
#include <stdio.h>

#include "ad9833.h"
#include "encodeur.h"
#include "epot.h"
#include "hardware_config.h"
#include "i2c_commun.h"
#include "integration.h"
#include "menu.h"
#include "oled.h"
#include "signaux.h"

static void initialiser_ad9833(void)
{
    const ad9833_config_t config = {
        .host = SYSTEM_AD9833_SPI_HOST,
        .mosi_gpio = SYSTEM_AD9833_MOSI_GPIO,
        .sclk_gpio = SYSTEM_AD9833_SCLK_GPIO,
        .fsync_gpio = SYSTEM_AD9833_FSYNC_GPIO,
        .spi_clock_hz = SYSTEM_AD9833_SPI_CLOCK_HZ,
        .mclk_hz = SYSTEM_AD9833_MCLK_HZ,
    };

    const esp_err_t err = Ad9833_Init(&config);
    if (err != ESP_OK)
    {
        printf("AD9833: initialisation impossible (%s). Menu disponible sans DDS.\n",
               esp_err_to_name(err));
        return;
    }

    (void)Ad9833_EnableOutput(false);
    printf("AD9833: driver SPI initialise, sortie maintenue OFF.\n");
}

static void initialiser_epot(void)
{
    const esp_err_t err = Epot_Init(i2c_commun_bus(),
                                    SYSTEM_EPOT_GAIN_I2C_ADDRESS,
                                    SYSTEM_EPOT_OFFSET_I2C_ADDRESS);
    if (err != ESP_OK)
    {
        printf("EPOT: initialisation impossible (%s).\n", esp_err_to_name(err));
        return;
    }

    printf("EPOT: drivers Gain/Offset enregistres, aucune consigne appliquee.\n");
}

static void afficher_diagnostic_demarrage(void)
{
    uint8_t eeprom_adresse = 0U;

    printf("\n=== DIAGNOSTIC MATERIEL ===\n");
    printf("I2C bus       : %s\n", integration_i2c_disponible() ? "OK" : "ABSENT");

    if (oled_est_present())
    {
        printf("OLED SH1106   : OK @ 0x%02X\n", oled_adresse());
    }
    else
    {
        printf("OLED SH1106   : ABSENT\n");
    }

    printf("EPOT Gain     : %s @ 0x%02X\n",
           integration_epot_gain_present() ? "ACK" : "ABSENT",
           SYSTEM_EPOT_GAIN_I2C_ADDRESS);
    printf("EPOT Offset   : %s @ 0x%02X\n",
           integration_epot_offset_present() ? "ACK" : "ABSENT",
           SYSTEM_EPOT_OFFSET_I2C_ADDRESS);

    if (integration_eeprom_detectee(&eeprom_adresse))
    {
        printf("EEPROM 24LC32: ACK @ 0x%02X (driver sauvegarde non integre)\n",
               eeprom_adresse);
    }
    else
    {
        printf("EEPROM 24LC32: ABSENTE sur 0x50..0x57\n");
    }

    printf("AD9833        : %s (SPI sans retour MISO)\n",
           integration_ad9833_disponible() ? "DRIVER INIT" : "DRIVER ABSENT");
    printf("Encodeur      : IRQ A/B actives, droite=bas, gauche=haut\n");
    printf("USB RX/TX     : NON INTEGRE\n");
    printf("VOUT          : OFF / activation bloquee avant calibration\n");
    printf("LT1206/TRACO  : test manuel uniquement\n");
    printf("============================\n\n");
}

void app_main(void)
{
    printf("\n=== GENERATEUR DE FONCTIONS - INTEGRATION UI V4 ===\n");

    signaux_initialiser();

    const esp_err_t i2c_err = i2c_commun_initialiser();
    if (i2c_err != ESP_OK)
    {
        printf("I2C: initialisation impossible (%s).\n", esp_err_to_name(i2c_err));
    }

    if (!oled_initialiser())
    {
        printf("OLED non detecte; verifier SDA/SCL et 3V3.\n");
    }

    encodeur_initialiser();

    if (i2c_commun_bus() != NULL)
    {
        initialiser_epot();
    }

    initialiser_ad9833();
    integration_initialiser();
    afficher_diagnostic_demarrage();
    menu_initialiser();

    while (1)
    {
        const evenement_ui_t evenement = encodeur_lire_evenement();
        if (evenement != UI_AUCUN)
        {
            menu_traiter(evenement);
        }
        menu_periodique();
    }
}

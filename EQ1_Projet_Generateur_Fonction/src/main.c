/* Generateur de fonctions CFPT - integration interface V3.
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
#include "initialisation.h"
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
    printf("AD9833: driver initialise, sortie maintenue OFF.\n");
}

static void initialiser_epot(void)
{
    const esp_err_t err = Epot_Init(i2c_commun_bus(),
                                    SYSTEM_EPOT_GAIN_I2C_ADDRESS,
                                    SYSTEM_EPOT_OFFSET_I2C_ADDRESS);
    if (err != ESP_OK)
    {
        printf("EPOT: detection/initialisation impossible (%s).\n", esp_err_to_name(err));
        return;
    }

    printf("EPOT: Gain 0x%02X et Offset 0x%02X detectes et enregistres.\n",
           SYSTEM_EPOT_GAIN_I2C_ADDRESS,
           SYSTEM_EPOT_OFFSET_I2C_ADDRESS);
}

void app_main(void)
{
    printf("\n=== GENERATEUR DE FONCTIONS - INTEGRATION UI ===\n");

    signaux_initialiser();

    const esp_err_t i2c_err = i2c_commun_initialiser();
    if (i2c_err != ESP_OK)
    {
        printf("I2C: initialisation impossible (%s).\n", esp_err_to_name(i2c_err));
    }
    else
    {
        /*
         * Scan AVANT l'enregistrement des drivers OLED/EPOT.
         * Cela donne un diagnostic brut et indépendant des handles de périphériques.
         */
        System_ScanI2cBus();
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

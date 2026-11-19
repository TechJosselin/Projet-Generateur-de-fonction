/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : MAIN.C
 * DESCRIPTIF : Point d'entree et supervision periodique.
 *==================================================================================================*/
#include "integration.h"
#include "encodeur.h"
#include "oled.h"
#include "menu.h"
#include "usb_service.h"
#include "usb_transport.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "temps.h"

/*============================== VARIABLES ET CONFIGURATION =======================================*/
/* Un tick de boucle laisse progresser les autres taches FreeRTOS. */
#define BOUCLE_PRINCIPALE_PAUSE_TICKS 1U

/*==================================== FONCTIONS ================================================*/
/*================================ INTERFACE PUBLIQUE ============================================*/
/*--------------------------------------------------------------------------------------------------
 * Fonction   : app_main()
 * Descriptif : Initialise les services et execute la boucle principale.
 *--------------------------------------------------------------------------------------------------*/
void app_main(void)
{
    integration_esp32_initialiser();
    (void) oled_initialiser();
    (void) encodeur_initialiser();
    menu_initialiser();
    usb_service_initialiser();
    (void) usb_transport_initialiser();
    for (;;)
    {
        uint32_t maintenant_ms =(uint32_t)(esp_timer_get_time() / TEMPS_US_PAR_MS);
        /* Un seul proprietaire du modele : aucune tache USB/EEPROM concurrente. */
        menu_traiter(encodeur_lire_evenement());
        usb_transport_periodique(maintenant_ms);
        integration_periodique(maintenant_ms);
        menu_periodique(maintenant_ms);
        oled_periodique(maintenant_ms);
        vTaskDelay(BOUCLE_PRINCIPALE_PAUSE_TICKS);
    }
}

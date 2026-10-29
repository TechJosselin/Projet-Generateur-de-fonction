/* Boucle unique du projet, sans modification du rendu OLED. */
#include "integration.h"
#include "encodeur.h"
#include "oled.h"
#include "menu.h"
#include "usb_service.h"
#include "usb_transport.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

void app_main(void)
{
    integration_esp32_initialiser();
    (void)oled_initialiser();
    encodeur_initialiser();
    menu_initialiser();
    usb_service_initialiser();
    (void)usb_transport_initialiser();

    for (;;)
    {
        const uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        menu_traiter(encodeur_lire_evenement());
        usb_transport_periodique(now);
        integration_periodique(now);
        menu_periodique(); /* Rendu original, titre fixe et 3 entrees. */
        vTaskDelay(1);
    }
}

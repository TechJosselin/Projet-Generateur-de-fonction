/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : ESP_TIMER.H
 * DESCRIPTIF : Simulation de la source de temps ESP-IDF.
 *==================================================================================================*/
#ifndef FAKE_ESP_TIMER_H_INCLUDED
#define FAKE_ESP_TIMER_H_INCLUDED

#include <stdint.h>

/*================================= DONNEES DE SIMULATION =========================================*/
int64_t esp_timer_get_time(void);

#endif /* FAKE_ESP_TIMER_H_INCLUDED */

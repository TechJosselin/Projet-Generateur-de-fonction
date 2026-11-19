/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : FREERTOS/TASK.H
 * DESCRIPTIF : Simulation du delai de tache FreeRTOS.
 *==================================================================================================*/
#ifndef FAKE_FREERTOS_TASK_H_INCLUDED
#define FAKE_FREERTOS_TASK_H_INCLUDED

#include "freertos/FreeRTOS.h"

/*================================= DONNEES DE SIMULATION =========================================*/
void vTaskDelay(TickType_t t);

#endif /* FAKE_FREERTOS_TASK_H_INCLUDED */

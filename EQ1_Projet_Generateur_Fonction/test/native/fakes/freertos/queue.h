/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : FREERTOS/QUEUE.H
 * DESCRIPTIF : Interface de file FreeRTOS remplacee par un faux pilote.
 *==================================================================================================*/
#ifndef FAKE_FREERTOS_QUEUE_H_INCLUDED
#define FAKE_FREERTOS_QUEUE_H_INCLUDED

#include "freertos/FreeRTOS.h"

/*================================= DONNEES DE SIMULATION =========================================*/
typedef void *QueueHandle_t;
QueueHandle_t xQueueCreate(unsigned n, unsigned size);
void vQueueDelete(QueueHandle_t q);
BaseType_t xQueueSendFromISR(QueueHandle_t q, const void *p, BaseType_t *wake);
BaseType_t xQueueReceive(QueueHandle_t q, void *p, TickType_t t);
void xQueueReset(QueueHandle_t q);

#endif /* FAKE_FREERTOS_QUEUE_H_INCLUDED */

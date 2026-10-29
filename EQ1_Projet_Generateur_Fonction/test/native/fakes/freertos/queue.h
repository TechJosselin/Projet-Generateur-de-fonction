#include "freertos/FreeRTOS.h"
typedef void *QueueHandle_t;
QueueHandle_t xQueueCreate(unsigned n,unsigned size);
void vQueueDelete(QueueHandle_t q);
BaseType_t xQueueSendFromISR(QueueHandle_t q,const void *p,BaseType_t *wake);
BaseType_t xQueueReceive(QueueHandle_t q,void *p,TickType_t t);
void xQueueReset(QueueHandle_t q);

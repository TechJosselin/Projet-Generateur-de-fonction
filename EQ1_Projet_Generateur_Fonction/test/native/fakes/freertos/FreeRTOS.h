#ifndef FREERTOS_H
#define FREERTOS_H
#include <stddef.h>
#include <stdbool.h>
typedef unsigned TickType_t;
typedef int BaseType_t;
typedef int portMUX_TYPE;
#define pdMS_TO_TICKS(x) ((unsigned)(x)/10)
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
#define portENTER_CRITICAL_ISR(x) ((void)(x))
#define portEXIT_CRITICAL_ISR(x) ((void)(x))
#define portYIELD_FROM_ISR() ((void)0)
#define pdFALSE 0
#define pdTRUE 1
#endif

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main()
{
    while (1)
    {
        printf("ESP32-C6 OK !\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
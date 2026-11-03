#ifndef AD9833_H
#define AD9833_H
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
typedef enum {AD9833_WAVE_SINE=0,AD9833_WAVE_TRIANGLE,AD9833_WAVE_SQUARE} ad9833_waveform_t;
typedef struct {
    spi_host_device_t host;
    gpio_num_t mosi_gpio,sclk_gpio,fsync_gpio;
    uint32_t spi_clock_hz,mclk_hz;
} ad9833_config_t;
esp_err_t Ad9833_Init(const ad9833_config_t *config);
esp_err_t Ad9833_Deinit(void);
/* Configure sous RESET, laisse la sortie arretee meme si elle etait active. */
esp_err_t Ad9833_SetSignal(ad9833_waveform_t waveform,double frequency_hz);
esp_err_t Ad9833_EnableOutput(bool enable);
bool Ad9833_IsInitialized(void);
bool Ad9833_IsOutputEnabled(void);
bool Ad9833_IsOutputStateKnown(void);
bool Ad9833_IsConfigurationValid(void);
double Ad9833_GetFrequencyHz(void);
ad9833_waveform_t Ad9833_GetWaveform(void);
#endif

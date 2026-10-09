/*===============================================================================================
CFPT - Projet : Générateur de fonction
Fichier      : ad9833.c
Description  : Driver SPI du générateur DDS AD9833

Base d'intégration : travail de Samuel (RDMR_GenerationSignal).
Le driver ne gère que l'AD9833 : fréquence, forme d'onde et activation de sortie.
===============================================================================================*/

#include "ad9833.h"

#include <stddef.h>

#define AD9833_REG_FREQ0            0x4000U

#define AD9833_CTRL_B28             (1U << 13)
#define AD9833_CTRL_RESET           (1U << 8)
#define AD9833_CTRL_OPBITEN         (1U << 5)
#define AD9833_CTRL_DIV2            (1U << 3)
#define AD9833_CTRL_MODE            (1U << 1)

#define AD9833_FREQ_WORD_SCALE      268435456.0 /* 2^28 */

static spi_host_device_t gHost = SPI2_HOST;
static spi_device_handle_t gDeviceHandle = NULL;
static bool gOwnsSpiBus = false;
static bool gInitialized = false;
static bool gOutputEnabled = false;
static uint32_t gMclkHz = 0U;
static uint16_t gControlRegister = AD9833_CTRL_B28 | AD9833_CTRL_RESET;
static double gFrequencyHz = 0.0;
static ad9833_waveform_t gWaveform = AD9833_WAVE_SINE;

static esp_err_t Ad9833_WriteRegister(uint16_t value)
{
    if (gDeviceHandle == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    const uint8_t tx_data[2] =
    {
        (uint8_t)((value >> 8U) & 0xFFU),
        (uint8_t)(value & 0xFFU)
    };

    spi_transaction_t transaction =
    {
        .length = 16U,
        .tx_buffer = tx_data,
    };

    return spi_device_transmit(gDeviceHandle, &transaction);
}

static esp_err_t Ad9833_GetWaveformBits(ad9833_waveform_t waveform, uint16_t *bits)
{
    if (bits == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    switch (waveform)
    {
        case AD9833_WAVE_SINE:
            *bits = 0U;
            return ESP_OK;

        case AD9833_WAVE_TRIANGLE:
            *bits = AD9833_CTRL_MODE;
            return ESP_OK;

        case AD9833_WAVE_SQUARE:
            *bits = AD9833_CTRL_OPBITEN | AD9833_CTRL_DIV2;
            return ESP_OK;

        default:
            return ESP_ERR_INVALID_ARG;
    }
}

esp_err_t Ad9833_Init(const ad9833_config_t *config)
{
    esp_err_t err;

    if (gInitialized)
    {
        return ESP_OK;
    }

    if ((config == NULL) ||
        !GPIO_IS_VALID_OUTPUT_GPIO(config->mosi_gpio) ||
        !GPIO_IS_VALID_OUTPUT_GPIO(config->sclk_gpio) ||
        !GPIO_IS_VALID_OUTPUT_GPIO(config->fsync_gpio) ||
        (config->spi_clock_hz == 0U) ||
        (config->mclk_hz == 0U))
    {
        return ESP_ERR_INVALID_ARG;
    }

    const spi_bus_config_t bus_config =
    {
        .mosi_io_num = config->mosi_gpio,
        .miso_io_num = -1,
        .sclk_io_num = config->sclk_gpio,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 2,
    };

    err = spi_bus_initialize(config->host, &bus_config, SPI_DMA_CH_AUTO);
    if (err == ESP_OK)
    {
        gOwnsSpiBus = true;
    }
    else if (err == ESP_ERR_INVALID_STATE)
    {
        /* Le bus est déjà initialisé : on ajoute simplement l'AD9833. */
        gOwnsSpiBus = false;
    }
    else
    {
        return err;
    }

    const spi_device_interface_config_t device_config =
    {
        .clock_speed_hz = (int)config->spi_clock_hz,
        .mode = 2,
        .spics_io_num = config->fsync_gpio,
        .queue_size = 1,
    };

    err = spi_bus_add_device(config->host, &device_config, &gDeviceHandle);
    if (err != ESP_OK)
    {
        if (gOwnsSpiBus)
        {
            spi_bus_free(config->host);
        }
        gOwnsSpiBus = false;
        gDeviceHandle = NULL;
        return err;
    }

    gHost = config->host;
    gMclkHz = config->mclk_hz;
    gControlRegister = AD9833_CTRL_B28 | AD9833_CTRL_RESET;
    gFrequencyHz = 0.0;
    gWaveform = AD9833_WAVE_SINE;
    gOutputEnabled = false;

    err = Ad9833_WriteRegister(gControlRegister);
    if (err != ESP_OK)
    {
        spi_bus_remove_device(gDeviceHandle);
        gDeviceHandle = NULL;
        if (gOwnsSpiBus)
        {
            spi_bus_free(gHost);
        }
        gOwnsSpiBus = false;
        gMclkHz = 0U;
        return err;
    }

    gInitialized = true;
    return ESP_OK;
}

esp_err_t Ad9833_Deinit(void)
{
    esp_err_t first_error = ESP_OK;

    if (!gInitialized)
    {
        return ESP_OK;
    }

    if (gDeviceHandle != NULL)
    {
        const esp_err_t err = spi_bus_remove_device(gDeviceHandle);
        if (err != ESP_OK)
        {
            first_error = err;
        }
        gDeviceHandle = NULL;
    }

    if (gOwnsSpiBus)
    {
        const esp_err_t err = spi_bus_free(gHost);
        if ((err != ESP_OK) && (first_error == ESP_OK))
        {
            first_error = err;
        }
    }

    gOwnsSpiBus = false;
    gInitialized = false;
    gOutputEnabled = false;
    gMclkHz = 0U;
    gControlRegister = AD9833_CTRL_B28 | AD9833_CTRL_RESET;
    gFrequencyHz = 0.0;
    gWaveform = AD9833_WAVE_SINE;

    return first_error;
}

esp_err_t Ad9833_SetSignal(ad9833_waveform_t waveform, double frequency_hz)
{
    esp_err_t err;
    uint16_t waveform_bits;

    if (!gInitialized || (gDeviceHandle == NULL) || (gMclkHz == 0U))
    {
        return ESP_ERR_INVALID_STATE;
    }

    if ((frequency_hz < 0.0) ||
        (frequency_hz > ((double)gMclkHz / 2.0)))
    {
        return ESP_ERR_INVALID_ARG;
    }

    err = Ad9833_GetWaveformBits(waveform, &waveform_bits);
    if (err != ESP_OK)
    {
        return err;
    }

    const uint32_t frequency_word = (uint32_t)(
        ((frequency_hz * AD9833_FREQ_WORD_SCALE) / (double)gMclkHz) + 0.5);

    const uint16_t frequency_lsb = (uint16_t)(frequency_word & 0x3FFFU);
    const uint16_t frequency_msb = (uint16_t)((frequency_word >> 14U) & 0x3FFFU);

    gControlRegister = AD9833_CTRL_B28 | AD9833_CTRL_RESET;

    err = Ad9833_WriteRegister(gControlRegister);
    if (err != ESP_OK)
    {
        return err;
    }

    err = Ad9833_WriteRegister((uint16_t)(AD9833_REG_FREQ0 | frequency_lsb));
    if (err != ESP_OK)
    {
        return err;
    }

    err = Ad9833_WriteRegister((uint16_t)(AD9833_REG_FREQ0 | frequency_msb));
    if (err != ESP_OK)
    {
        return err;
    }

    gControlRegister = AD9833_CTRL_B28 | waveform_bits;
    err = Ad9833_WriteRegister(gControlRegister);
    if (err != ESP_OK)
    {
        return err;
    }

    gFrequencyHz = frequency_hz;
    gWaveform = waveform;
    gOutputEnabled = true;

    return ESP_OK;
}

esp_err_t Ad9833_EnableOutput(bool enable)
{
    esp_err_t err;
    uint16_t new_control;

    if (!gInitialized || (gDeviceHandle == NULL))
    {
        return ESP_ERR_INVALID_STATE;
    }

    new_control = gControlRegister;

    if (enable)
    {
        new_control &= (uint16_t)~AD9833_CTRL_RESET;
    }
    else
    {
        new_control |= AD9833_CTRL_RESET;
    }

    err = Ad9833_WriteRegister(new_control);
    if (err == ESP_OK)
    {
        gControlRegister = new_control;
        gOutputEnabled = enable;
    }

    return err;
}

bool Ad9833_IsInitialized(void)
{
    return gInitialized;
}

bool Ad9833_IsOutputEnabled(void)
{
    return gOutputEnabled;
}

double Ad9833_GetFrequencyHz(void)
{
    return gFrequencyHz;
}

ad9833_waveform_t Ad9833_GetWaveform(void)
{
    return gWaveform;
}

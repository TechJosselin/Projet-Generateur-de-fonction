/*===============================================================================================
CFPT - Projet : Générateur de fonction
Fichier      : oled.c
Description  : Driver applicatif de l'écran OLED SH1106 128x64 I2C
===============================================================================================*/

#include <stdio.h>
#include <string.h>

#include "oled.h"
#include "hardware_config.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_sh1106.h"

/*===============================================================================================
VARIABLES LOCALES
===============================================================================================*/
static esp_lcd_panel_io_handle_t gOledIoHandle = NULL;
static esp_lcd_panel_handle_t gOledPanelHandle = NULL;
static bool gOledInitialized = false;
static uint8_t gOledI2cAddress = 0U;
static uint8_t gOledFrameBuffer[(SYSTEM_OLED_WIDTH * SYSTEM_OLED_HEIGHT) / 8U];

/* Police minimale 5x7 pour les chiffres 0..9. Chaque octet représente une ligne sur 5 pixels. */
static const uint8_t gDigitGlyphs[10][7] =
{
    {0x0EU, 0x11U, 0x13U, 0x15U, 0x19U, 0x11U, 0x0EU}, /* 0 */
    {0x04U, 0x0CU, 0x04U, 0x04U, 0x04U, 0x04U, 0x0EU}, /* 1 */
    {0x0EU, 0x11U, 0x01U, 0x02U, 0x04U, 0x08U, 0x1FU}, /* 2 */
    {0x1EU, 0x01U, 0x01U, 0x0EU, 0x01U, 0x01U, 0x1EU}, /* 3 */
    {0x02U, 0x06U, 0x0AU, 0x12U, 0x1FU, 0x02U, 0x02U}, /* 4 */
    {0x1FU, 0x10U, 0x10U, 0x1EU, 0x01U, 0x01U, 0x1EU}, /* 5 */
    {0x0EU, 0x10U, 0x10U, 0x1EU, 0x11U, 0x11U, 0x0EU}, /* 6 */
    {0x1FU, 0x01U, 0x02U, 0x04U, 0x08U, 0x08U, 0x08U}, /* 7 */
    {0x0EU, 0x11U, 0x11U, 0x0EU, 0x11U, 0x11U, 0x0EU}, /* 8 */
    {0x0EU, 0x11U, 0x11U, 0x0FU, 0x01U, 0x01U, 0x0EU}  /* 9 */
};

static const uint8_t gMinusGlyph[7] =
{
    0x00U, 0x00U, 0x00U, 0x1FU, 0x00U, 0x00U, 0x00U
};

/*===============================================================================================
PROTOTYPES DE FONCTIONS LOCALES
===============================================================================================*/
static esp_err_t Oled_DetectAddress(i2c_master_bus_handle_t bus_handle, uint8_t *address);
static void Oled_SetPixel(uint16_t x, uint16_t y, bool enabled);
static void Oled_DrawCharacter(uint16_t x, uint16_t y, char character, uint8_t scale);
static void Oled_CleanupHandles(void);

/*===============================================================================================
FONCTIONS LOCALES
===============================================================================================*/
static esp_err_t Oled_DetectAddress(i2c_master_bus_handle_t bus_handle, uint8_t *address)
{
    if ((bus_handle == NULL) || (address == NULL))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (i2c_master_probe(bus_handle,
                         SYSTEM_OLED_I2C_ADDRESS_PRIMARY,
                         SYSTEM_OLED_PROBE_TIMEOUT_MS) == ESP_OK)
    {
        *address = SYSTEM_OLED_I2C_ADDRESS_PRIMARY;
        return ESP_OK;
    }

    if (i2c_master_probe(bus_handle,
                         SYSTEM_OLED_I2C_ADDRESS_SECONDARY,
                         SYSTEM_OLED_PROBE_TIMEOUT_MS) == ESP_OK)
    {
        *address = SYSTEM_OLED_I2C_ADDRESS_SECONDARY;
        return ESP_OK;
    }

    return ESP_ERR_NOT_FOUND;
}

static void Oled_SetPixel(uint16_t x, uint16_t y, bool enabled)
{
    if ((x >= SYSTEM_OLED_WIDTH) || (y >= SYSTEM_OLED_HEIGHT))
    {
        return;
    }

    const uint16_t page = y / 8U;
    const uint8_t bit = (uint8_t)(y % 8U);
    const uint16_t index = (page * SYSTEM_OLED_WIDTH) + x;

    if (enabled)
    {
        gOledFrameBuffer[index] |= (uint8_t)(1U << bit);
    }
    else
    {
        gOledFrameBuffer[index] &= (uint8_t)~(1U << bit);
    }
}

static void Oled_DrawCharacter(uint16_t x, uint16_t y, char character, uint8_t scale)
{
    const uint8_t *glyph = NULL;
    uint8_t row;
    uint8_t column;
    uint8_t sx;
    uint8_t sy;

    if ((character >= '0') && (character <= '9'))
    {
        glyph = gDigitGlyphs[(uint8_t)(character - '0')];
    }
    else if (character == '-')
    {
        glyph = gMinusGlyph;
    }
    else
    {
        return;
    }

    for (row = 0U; row < 7U; row++)
    {
        for (column = 0U; column < 5U; column++)
        {
            const bool enabled = (glyph[row] & (uint8_t)(1U << (4U - column))) != 0U;

            if (!enabled)
            {
                continue;
            }

            for (sy = 0U; sy < scale; sy++)
            {
                for (sx = 0U; sx < scale; sx++)
                {
                    Oled_SetPixel((uint16_t)(x + (column * scale) + sx),
                                  (uint16_t)(y + (row * scale) + sy),
                                  true);
                }
            }
        }
    }
}

static void Oled_CleanupHandles(void)
{
    if (gOledPanelHandle != NULL)
    {
        esp_lcd_panel_del(gOledPanelHandle);
        gOledPanelHandle = NULL;
    }

    if (gOledIoHandle != NULL)
    {
        esp_lcd_panel_io_del(gOledIoHandle);
        gOledIoHandle = NULL;
    }

    gOledInitialized = false;
    gOledI2cAddress = 0U;
}

/*===============================================================================================
FONCTIONS PUBLIQUES
===============================================================================================*/
esp_err_t Oled_Init(i2c_master_bus_handle_t bus_handle)
{
    esp_err_t err;
    uint8_t detected_address = 0U;

    if (bus_handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (gOledInitialized)
    {
        return ESP_OK;
    }

    err = Oled_DetectAddress(bus_handle, &detected_address);
    if (err != ESP_OK)
    {
        return err;
    }

    const esp_lcd_panel_io_i2c_config_t io_config =
    {
        .dev_addr = detected_address,
        .scl_speed_hz = SYSTEM_OLED_I2C_SPEED_HZ,
        .control_phase_bytes = 1,
        .dc_bit_offset = 6,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };

    err = esp_lcd_new_panel_io_i2c(bus_handle, &io_config, &gOledIoHandle);
    if (err != ESP_OK)
    {
        Oled_CleanupHandles();
        return err;
    }

    const esp_lcd_panel_dev_config_t panel_config =
    {
        .reset_gpio_num = -1,
        .bits_per_pixel = 1,
    };

    err = esp_lcd_new_panel_sh1106(gOledIoHandle, &panel_config, &gOledPanelHandle);
    if (err != ESP_OK)
    {
        Oled_CleanupHandles();
        return err;
    }

    err = esp_lcd_panel_reset(gOledPanelHandle);
    if (err != ESP_OK)
    {
        Oled_CleanupHandles();
        return err;
    }

    err = esp_lcd_panel_init(gOledPanelHandle);
    if (err != ESP_OK)
    {
        Oled_CleanupHandles();
        return err;
    }

    err = esp_lcd_panel_disp_on_off(gOledPanelHandle, true);
    if (err != ESP_OK)
    {
        Oled_CleanupHandles();
        return err;
    }

    gOledI2cAddress = detected_address;
    gOledInitialized = true;

    return Oled_Clear();
}

esp_err_t Oled_Deinit(void)
{
    esp_err_t first_error = ESP_OK;
    esp_err_t err;

    if (!gOledInitialized)
    {
        return ESP_OK;
    }

    if (gOledPanelHandle != NULL)
    {
        err = esp_lcd_panel_disp_on_off(gOledPanelHandle, false);
        if ((err != ESP_OK) && (first_error == ESP_OK))
        {
            first_error = err;
        }

        err = esp_lcd_panel_del(gOledPanelHandle);
        if ((err != ESP_OK) && (first_error == ESP_OK))
        {
            first_error = err;
        }
        gOledPanelHandle = NULL;
    }

    if (gOledIoHandle != NULL)
    {
        err = esp_lcd_panel_io_del(gOledIoHandle);
        if ((err != ESP_OK) && (first_error == ESP_OK))
        {
            first_error = err;
        }
        gOledIoHandle = NULL;
    }

    gOledInitialized = false;
    gOledI2cAddress = 0U;

    return first_error;
}

esp_err_t Oled_Clear(void)
{
    if (!gOledInitialized || (gOledPanelHandle == NULL))
    {
        return ESP_ERR_INVALID_STATE;
    }

    memset(gOledFrameBuffer, 0, sizeof(gOledFrameBuffer));

    return esp_lcd_panel_draw_bitmap(gOledPanelHandle,
                                     0,
                                     0,
                                     SYSTEM_OLED_WIDTH,
                                     SYSTEM_OLED_HEIGHT,
                                     gOledFrameBuffer);
}

esp_err_t Oled_TestPattern(void)
{
    uint16_t x;
    uint16_t y;

    if (!gOledInitialized || (gOledPanelHandle == NULL))
    {
        return ESP_ERR_INVALID_STATE;
    }

    memset(gOledFrameBuffer, 0, sizeof(gOledFrameBuffer));

    for (x = 0U; x < SYSTEM_OLED_WIDTH; x++)
    {
        Oled_SetPixel(x, 0U, true);
        Oled_SetPixel(x, SYSTEM_OLED_HEIGHT - 1U, true);
    }

    for (y = 0U; y < SYSTEM_OLED_HEIGHT; y++)
    {
        Oled_SetPixel(0U, y, true);
        Oled_SetPixel(SYSTEM_OLED_WIDTH - 1U, y, true);
    }

    for (x = 0U; x < SYSTEM_OLED_WIDTH; x++)
    {
        Oled_SetPixel(x, SYSTEM_OLED_HEIGHT / 2U, true);
    }

    for (y = 0U; y < SYSTEM_OLED_HEIGHT; y++)
    {
        Oled_SetPixel(SYSTEM_OLED_WIDTH / 2U, y, true);
    }

    return esp_lcd_panel_draw_bitmap(gOledPanelHandle,
                                     0,
                                     0,
                                     SYSTEM_OLED_WIDTH,
                                     SYSTEM_OLED_HEIGHT,
                                     gOledFrameBuffer);
}

esp_err_t Oled_ShowCounter(int32_t value)
{
    char value_text[16];
    size_t length;
    uint8_t scale;
    uint16_t character_width;
    uint16_t spacing;
    uint16_t total_width;
    uint16_t x;
    uint16_t y;
    size_t i;

    if (!gOledInitialized || (gOledPanelHandle == NULL))
    {
        return ESP_ERR_INVALID_STATE;
    }

    snprintf(value_text, sizeof(value_text), "%ld", (long)value);
    length = strlen(value_text);

    if (length <= 5U)
    {
        scale = 3U;
    }
    else if (length <= 10U)
    {
        scale = 2U;
    }
    else
    {
        scale = 1U;
    }

    character_width = (uint16_t)(5U * scale);
    spacing = scale;
    total_width = (uint16_t)((length * character_width) + ((length - 1U) * spacing));

    x = (SYSTEM_OLED_WIDTH > total_width) ?
        (uint16_t)((SYSTEM_OLED_WIDTH - total_width) / 2U) : 0U;
    y = (uint16_t)((SYSTEM_OLED_HEIGHT - (7U * scale)) / 2U);

    memset(gOledFrameBuffer, 0, sizeof(gOledFrameBuffer));

    for (i = 0U; i < length; i++)
    {
        Oled_DrawCharacter(x, y, value_text[i], scale);
        x = (uint16_t)(x + character_width + spacing);
    }

    return esp_lcd_panel_draw_bitmap(gOledPanelHandle,
                                     0,
                                     0,
                                     SYSTEM_OLED_WIDTH,
                                     SYSTEM_OLED_HEIGHT,
                                     gOledFrameBuffer);
}

bool Oled_IsInitialized(void)
{
    return gOledInitialized;
}

uint8_t Oled_GetI2cAddress(void)
{
    return gOledI2cAddress;
}

/*===============================================================================================
FIN DU FICHIER
===============================================================================================*/

/*===============================================================================================
CFPT - Projet : Générateur de fonction
Fichier      : hardware_config.h
Description  : Configuration matérielle commune du XIAO ESP32-C6
===============================================================================================*/

#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

/*===============================================================================================
INCLUDES
===============================================================================================*/
#include "driver/gpio.h"
#include "driver/i2c_master.h"

/*===============================================================================================
LED DE STATUT
===============================================================================================*/
#define SYSTEM_STATUS_LED_GPIO                  GPIO_NUM_15
#define SYSTEM_STATUS_LED_ON_LEVEL              0
#define SYSTEM_STATUS_LED_OFF_LEVEL             1

/*===============================================================================================
BUS I2C PARTAGE
===============================================================================================*/
/*
 * Bus commun utilisé par :
 *   - Ecran OLED SH1106
 *   - EEPROM 24LC32
 *   - EPOT Gain MCP45HV51
 *   - EPOT Offset MCP45HV51
 */
#define SYSTEM_I2C_PORT                         I2C_NUM_0
#define SYSTEM_I2C_SDA_GPIO                     GPIO_NUM_22
#define SYSTEM_I2C_SCL_GPIO                     GPIO_NUM_23
#define SYSTEM_I2C_GLITCH_IGNORE_COUNT          7U
#define SYSTEM_I2C_ENABLE_INTERNAL_PULLUPS      1

/*===============================================================================================
ECRAN OLED SH1106 1.3\" I2C
===============================================================================================*/
/*
 * Le module photographié possède des straps pour le brochage d'alimentation :
 *   - pin 1 -> GND
 *   - pin 2 -> VDD
 * Ces straps ne sélectionnent pas l'adresse I2C.
 *
 * Le logiciel teste automatiquement les deux adresses usuelles du SH1106.
 */
#define SYSTEM_OLED_I2C_ADDRESS_PRIMARY         0x3CU
#define SYSTEM_OLED_I2C_ADDRESS_SECONDARY       0x3DU
#define SYSTEM_OLED_I2C_SPEED_HZ                400000U
#define SYSTEM_OLED_WIDTH                       128U
#define SYSTEM_OLED_HEIGHT                      64U
#define SYSTEM_OLED_PROBE_TIMEOUT_MS            50U

/*===============================================================================================
ENCODEUR ROTATIF PEC12R-4220F-S0024
===============================================================================================*/
/*
 * Câblage XIAO ESP32-C6 utilisé pour le prototype :
 *   - A / CLK  -> D1 / GPIO1
 *   - B / DT   -> D2 / GPIO2
 *   - Commun   -> GND
 *   - SW       -> D0 / GPIO0
 *   - SW GND   -> GND
 *
 * Les entrées utilisent les pull-up internes de l'ESP32-C6.
 * Le décodage quadrature est réalisé sous interruption sur A et B.
 */
#define SYSTEM_ENCODER_A_GPIO                   GPIO_NUM_1
#define SYSTEM_ENCODER_B_GPIO                   GPIO_NUM_2
#define SYSTEM_ENCODER_BUTTON_GPIO              GPIO_NUM_0
#define SYSTEM_ENCODER_EDGES_PER_STEP           4U
#define SYSTEM_ENCODER_REVERSE_DIRECTION        0
#define SYSTEM_ENCODER_POLL_PERIOD_MS           10U

/*===============================================================================================
POTENTIOMETRES NUMERIQUES MCP45HV51
===============================================================================================*/
#define SYSTEM_EPOT_GAIN_I2C_ADDRESS            0x3EU
#define SYSTEM_EPOT_OFFSET_I2C_ADDRESS          0x3FU
#define SYSTEM_EPOT_TEST_RAW_VALUE              127U

#endif /* HARDWARE_CONFIG_H */

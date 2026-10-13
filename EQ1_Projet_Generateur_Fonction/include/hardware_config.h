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
#include "driver/spi_master.h"

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
#define SYSTEM_I2C_DIAG_PROBE_TIMEOUT_MS        5

/*===============================================================================================
ECRAN OLED SH1106 1.3\" I2C
===============================================================================================*/
#define SYSTEM_OLED_I2C_ADDRESS_PRIMARY         0x3CU
#define SYSTEM_OLED_I2C_ADDRESS_SECONDARY       0x3DU
#define SYSTEM_OLED_I2C_SPEED_HZ                100000U
#define SYSTEM_OLED_WIDTH                       128U
#define SYSTEM_OLED_HEIGHT                      64U
#define SYSTEM_OLED_PROBE_TIMEOUT_MS            50U

/*===============================================================================================
EEPROM 24LC32
===============================================================================================*/
/*
 * L'adresse 7 bits dépend des entrées matérielles A0/A1/A2.
 * Le diagnostic recherche donc automatiquement l'EEPROM dans toute la plage 0x50..0x57.
 * Le driver de sauvegarde/chargement n'est pas encore intégré : cette plage sert uniquement
 * à vérifier la présence physique de la mémoire sur le bus I2C.
 */
#define SYSTEM_EEPROM_I2C_ADDRESS_MIN            0x50U
#define SYSTEM_EEPROM_I2C_ADDRESS_MAX            0x57U

/*===============================================================================================
ENCODEUR ROTATIF PEC12R-4220F-S0024
===============================================================================================*/
/*
 * Câblage XIAO ESP32-C6 utilisé par le projet :
 *   - A / CLK  -> D1 / GPIO1
 *   - B / DT   -> D2 / GPIO2
 *   - Commun   -> GND
 *   - SW       -> D0 / GPIO0
 *   - SW GND   -> GND
 *
 * Le driver utilise des interruptions sur A et B (tous les fronts) et un décodeur
 * quadrature complet. Un déplacement n'est validé qu'au retour sur une position
 * de détente après une séquence complète, ce qui filtre les rebonds mécaniques.
 * Le sens logique est inversé pour obtenir : droite = descendre, gauche = remonter.
 */
#define SYSTEM_ENCODER_A_GPIO                   GPIO_NUM_1
#define SYSTEM_ENCODER_B_GPIO                   GPIO_NUM_2
#define SYSTEM_ENCODER_BUTTON_GPIO              GPIO_NUM_0
#define SYSTEM_ENCODER_REVERSE_DIRECTION        1

/*===============================================================================================
GENERATEUR DDS AD9833
===============================================================================================*/
#define SYSTEM_AD9833_SPI_HOST                  SPI2_HOST
#define SYSTEM_AD9833_MOSI_GPIO                 GPIO_NUM_18
#define SYSTEM_AD9833_SCLK_GPIO                 GPIO_NUM_19
#define SYSTEM_AD9833_FSYNC_GPIO                GPIO_NUM_21
#define SYSTEM_AD9833_SPI_CLOCK_HZ              1000000U
#define SYSTEM_AD9833_MCLK_HZ                   25000000U
#define SYSTEM_AD9833_TEST_PRESET               1U

/*===============================================================================================
POTENTIOMETRES NUMERIQUES MCP45HV51
===============================================================================================*/
#define SYSTEM_EPOT_GAIN_I2C_ADDRESS            0x3FU
#define SYSTEM_EPOT_OFFSET_I2C_ADDRESS          0x3EU

#endif /* HARDWARE_CONFIG_H */

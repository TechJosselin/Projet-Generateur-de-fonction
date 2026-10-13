#ifndef ENCODEUR_H
#define ENCODEUR_H

#include "hardware_config.h"

/* Le brochage reste centralise dans hardware_config.h. */
#define PIN_ENC_A   SYSTEM_ENCODER_A_GPIO
#define PIN_ENC_B   SYSTEM_ENCODER_B_GPIO
#define PIN_ENC_SW  SYSTEM_ENCODER_BUTTON_GPIO

#define INVERSION_SENS_ENCODEUR SYSTEM_ENCODER_REVERSE_DIRECTION
#define TRANSITIONS_PAR_CRAN    SYSTEM_ENCODER_EDGES_PER_STEP

typedef enum
{
    UI_AUCUN = 0,
    UI_ROTATION_PLUS,
    UI_ROTATION_MOINS,
    UI_CLIC,
    UI_APPUI_LONG
} evenement_ui_t;

void encodeur_initialiser(void);
evenement_ui_t encodeur_lire_evenement(void);

#endif /* ENCODEUR_H */

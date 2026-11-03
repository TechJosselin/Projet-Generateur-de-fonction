#ifndef BOARD_OPTIONS_H
#define BOARD_OPTIONS_H
/* Aucun GPIO arbitraire : definir ces valeurs seulement apres revue du schema.
 * Le mode optionnel implemente est un PWM independant 3,3 V, PAS une exigence
 * deduite du schema bloc. -1 signifie non equipe/non configure. */
#ifndef SYSTEM_DIGITAL_1_GPIO
#define SYSTEM_DIGITAL_1_GPIO (-1)
#endif
#ifndef SYSTEM_DIGITAL_2_GPIO
#define SYSTEM_DIGITAL_2_GPIO (-1)
#endif
#endif

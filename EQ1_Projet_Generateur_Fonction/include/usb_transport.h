/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : USB_TRANSPORT.H
 * DESCRIPTIF : Transport de trames sur la liaison USB.
 *==================================================================================================*/
#ifndef USB_TRANSPORT_H
#define USB_TRANSPORT_H
#include <stdbool.h>
#include <stdint.h>

/*============================== VARIABLES ET CONFIGURATION =======================================*/
bool usb_transport_initialiser(void);
void usb_transport_periodique(uint32_t maintenant_ms);
bool usb_transport_disponible(void);
#endif

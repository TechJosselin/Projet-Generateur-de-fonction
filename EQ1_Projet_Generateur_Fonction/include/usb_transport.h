#ifndef USB_TRANSPORT_H
#define USB_TRANSPORT_H
#include <stdbool.h>
#include <stdint.h>
bool usb_transport_initialiser(void);
void usb_transport_periodique(uint32_t maintenant_ms);
bool usb_transport_disponible(void);
#endif

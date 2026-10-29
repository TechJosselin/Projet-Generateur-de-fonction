#ifndef USB_SERVICE_H
#define USB_SERVICE_H
#include "usb_protocol.h"
#define USB_TX_CAPACITE 8192U
void usb_service_initialiser(void);
void usb_service_octet(uint8_t octet,uint32_t maintenant_ms);
void usb_service_periodique(uint32_t maintenant_ms);
size_t usb_service_espace_tx(void);
const uint8_t *usb_service_tx(size_t *longueur);
void usb_service_tx_consomme(size_t longueur,uint32_t maintenant_ms);
bool usb_service_reinitialisation_transport(void);
typedef struct {uint32_t trames,invalides,depassements,abandons_rx,abandons_tx,octets_tx;} usb_stats_t;
const usb_stats_t *usb_service_statistiques(void);
#endif

#ifndef USB_PROTOCOL_H
#define USB_PROTOCOL_H
#include <stddef.h>
#include <stdbool.h>
/* Moteur de trames pur C, sans pilote USB physique.
 * Format: $N:TYPE:AMPL:OFFSET:FREQ#, $N?# et $?#.
 */
typedef enum {
    USB_PROTO_ERREUR=0,
    USB_PROTO_ECRITURE_RAM,
    USB_PROTO_LECTURE_UN,
    USB_PROTO_LECTURE_TOUT
} usb_proto_resultat_t;
usb_proto_resultat_t usb_proto_traiter(const char *trame,char *reponse,size_t taille);
bool usb_proto_formater_signal(size_t index,char *reponse,size_t taille);
#endif

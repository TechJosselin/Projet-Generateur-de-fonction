#ifndef USB_PROTOCOL_H
#define USB_PROTOCOL_H
#include "gf.h"
#include "signaux.h"
#define USB_TRAME_MAX 96U
typedef enum {USB_ECRIRE,USB_LIRE,USB_TOUT,USB_ETAT,USB_CHOISIR,USB_ON,USB_OFF,
    USB_SAUVER,USB_CHARGER,USB_USINE,USB_DUPLIQUER,USB_SUPPRIMER,USB_DIGITAL,
    USB_CAL_BEGIN,USB_CAL_DATA,USB_CAL_COMMIT,USB_CAL_ABORT,USB_CAL_CLEAR,USB_CAL_GET,USB_DIG_INFOS} usb_action_t;
typedef struct {
    usb_action_t action;
    size_t index;
    signal_config_t signal;
    uint32_t frequence;
    uint16_t rapport,position;
    bool actif;
    uint8_t donnees[32];
} usb_commande_t;
/* Analyse syntaxique seulement : aucune mutation, aucun acces materiel. */
gf_result_t usb_proto_decoder(const char *trame,usb_commande_t *commande);
bool usb_proto_formater(size_t index,const signal_config_t *s,char *dst,size_t taille);
bool usb_proto_formater_signal(size_t index,char *dst,size_t taille);
#endif

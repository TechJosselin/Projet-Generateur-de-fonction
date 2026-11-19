/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : USB_TRANSPORT.C
 * DESCRIPTIF : Transport de trames sur la liaison USB.
 *==================================================================================================*/
#include "usb_transport.h"
#include "usb_service.h"
#include "driver/usb_serial_jtag.h"

/*============================== VARIABLES ET CONFIGURATION =======================================*/
#define USB_TRANSPORT_BUFFER_TX_OCTETS     256U
#define USB_TRANSPORT_BUFFER_RX_OCTETS     1024U
#define USB_TRANSPORT_RECONNEXION_MS       1000U
#define USB_TRANSPORT_ESPACE_TX_MIN_OCTETS 4096U
#define USB_TRANSPORT_PAQUET_RX_OCTETS     64U
static bool pilote_installe;
static uint32_t dernier_essai_ms;
/*==================================== FONCTIONS ================================================*/
/*================================ INTERFACE PUBLIQUE ============================================*/
bool usb_transport_initialiser(void)
{
    if (pilote_installe)
    {
        return true;
    }
    usb_serial_jtag_driver_config_t config_usb = {
        .tx_buffer_size = USB_TRANSPORT_BUFFER_TX_OCTETS,
        .rx_buffer_size = USB_TRANSPORT_BUFFER_RX_OCTETS
    };
    pilote_installe = usb_serial_jtag_driver_install(&config_usb) == ESP_OK;
    return pilote_installe;
}

bool usb_transport_disponible(void)
{
    return pilote_installe;
}

void usb_transport_periodique(uint32_t maintenant_ms)
{
    usb_service_periodique(maintenant_ms);
    if (usb_service_reinitialisation_transport() && pilote_installe)
    {
        /* Ne pas conserver une ancienne reponse dans le tampon du pilote. */
        if (usb_serial_jtag_driver_uninstall() == ESP_OK)
        {
            pilote_installe = false;
        }
        dernier_essai_ms = maintenant_ms;
    }
    if (!pilote_installe)
    {
        if ((uint32_t)(maintenant_ms - dernier_essai_ms) >= USB_TRANSPORT_RECONNEXION_MS)
        {
            dernier_essai_ms = maintenant_ms;
            (void) usb_transport_initialiser();
        }
        return;
    }
    /* Lecture bornee : les autres services progressent meme sous un flot USB. */
    if (usb_service_espace_tx() > USB_TRANSPORT_ESPACE_TX_MIN_OCTETS)
    {
        uint8_t buffer_rx_usb[USB_TRANSPORT_PAQUET_RX_OCTETS];
        int longueur = usb_serial_jtag_read_bytes(buffer_rx_usb, sizeof(buffer_rx_usb), 0);
        for (int index_octet = 0; index_octet < longueur; index_octet++)
        {
            usb_service_octet(buffer_rx_usb[index_octet], maintenant_ms);
        }
    }
    size_t longueur = 0;
    const uint8_t *octets_tx = usb_service_tx(&longueur);
    if (longueur)
    {
        int nombre_emis = usb_serial_jtag_write_bytes(octets_tx, longueur, 0);
        if (nombre_emis > 0)
        {
            usb_service_tx_consomme((size_t) nombre_emis, maintenant_ms);
        }
    }
}

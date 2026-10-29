#include "usb_transport.h"
#include "usb_service.h"
#include "driver/usb_serial_jtag.h"
static bool installe;
static uint32_t dernier_essai;
bool usb_transport_initialiser(void) {
    if(installe)return true;
    usb_serial_jtag_driver_config_t c={.tx_buffer_size=256,.rx_buffer_size=1024};
    installe=usb_serial_jtag_driver_install(&c)==ESP_OK;return installe;
}
bool usb_transport_disponible(void){return installe;}
void usb_transport_periodique(uint32_t now) {
    usb_service_periodique(now);
    if(usb_service_reinitialisation_transport() && installe) {
        /* Ne pas conserver une ancienne reponse dans le tampon du pilote. */
        if(usb_serial_jtag_driver_uninstall()==ESP_OK)installe=false;
        dernier_essai=now;
    }
    if(!installe) {
        if((uint32_t)(now-dernier_essai)>=1000){dernier_essai=now;(void)usb_transport_initialiser();}
        return;
    }
    /* Lecture bornee : les autres services progressent meme sous un flot USB. */
    if(usb_service_espace_tx()>4096) {
        uint8_t b[64];int n=usb_serial_jtag_read_bytes(b,sizeof(b),0);
        for(int i=0;i<n;i++)usb_service_octet(b[i],now);
    }
    size_t n=0;const uint8_t *p=usb_service_tx(&n);
    if(n) {int w=usb_serial_jtag_write_bytes(p,n,0);if(w>0)usb_service_tx_consomme((size_t)w,now);}
}

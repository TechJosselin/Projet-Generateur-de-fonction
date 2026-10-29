# Liste explicite : ESP-IDF evalue aussi ce fichier en mode script pour les dependances.
idf_component_register(
    SRCS
        "ad9833.c" "analogique.c" "eeprom.c" "encodeur.c" "entree.c"
        "epot.c" "gf.c" "i2c_commun.c" "initialisation.c" "integration.c"
        "integration_esp32.c" "main.c" "menu.c" "oled.c" "signaux.c"
        "sorties_digitales.c" "stockage.c" "usb_protocol.c" "usb_service.c"
        "usb_transport.c"
    INCLUDE_DIRS "../include"
    REQUIRES esp_driver_gpio esp_driver_i2c esp_driver_spi esp_driver_ledc
             esp_driver_usb_serial_jtag esp_timer
)
# Avertissements sur notre composant, pas sur toutes les dependances ESP-IDF.
if(NOT CMAKE_SCRIPT_MODE_FILE)
    target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
endif()

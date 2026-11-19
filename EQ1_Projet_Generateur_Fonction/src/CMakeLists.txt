# ==============================================================================================
# CFPT - Equipe 1 - Generateur de fonctions
# Sources et dependances du composant applicatif ESP-IDF.
# Les tests natifs et leurs faux pilotes restent dans test/native.
# ==============================================================================================

idf_component_register(
    SRCS
        # Generation et adaptation analogique
        "ad9833.c"
        "analogique.c"
        "epot.c"
        "signaux.c"
        "sorties_digitales.c"

        # Bus materiels et stockage
        "eeprom.c"
        "i2c_commun.c"
        "stockage.c"

        # Interface utilisateur
        "encodeur.c"
        "entree.c"
        "menu.c"
        "oled.c"

        # Integration et supervision
        "gf.c"
        "initialisation.c"
        "integration.c"
        "integration_esp32.c"
        "main.c"

        # Protocole USB serie
        "usb_protocol.c"
        "usb_service.c"
        "usb_transport.c"

    INCLUDE_DIRS
        "../include"

    REQUIRES
        esp_driver_gpio
        esp_driver_i2c
        esp_driver_spi
        esp_driver_ledc
        esp_driver_usb_serial_jtag
        esp_timer
)

# Ces options de compilation ne concernent que le composant de l'equipe.
if(NOT CMAKE_SCRIPT_MODE_FILE)
    target_compile_options(
        ${COMPONENT_LIB}
        PRIVATE
            -Wall
            -Wextra
            -Werror
    )
endif()

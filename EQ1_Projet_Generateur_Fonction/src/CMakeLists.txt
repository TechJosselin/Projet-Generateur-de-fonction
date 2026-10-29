file(GLOB app_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/*.c")
idf_component_register(SRCS ${app_sources} INCLUDE_DIRS "../include"
    REQUIRES esp_driver_gpio esp_driver_i2c esp_driver_spi esp_driver_ledc
             esp_driver_usb_serial_jtag esp_timer)
# Avertissements sur notre composant, pas sur toutes les dependances ESP-IDF.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)

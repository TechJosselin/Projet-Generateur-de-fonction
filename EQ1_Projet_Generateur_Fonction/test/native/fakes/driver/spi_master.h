#ifndef DRIVER_SPI_H
#define DRIVER_SPI_H
#include <stdint.h>
#include "esp_err.h"
typedef int spi_host_device_t;
typedef void *spi_device_handle_t;
#define SPI2_HOST 1
#define SPI_DMA_CH_AUTO 3
#define SPI_TRANS_USE_TXDATA 4
typedef struct {int mosi_io_num,miso_io_num,sclk_io_num,quadwp_io_num,quadhd_io_num,max_transfer_sz;} spi_bus_config_t;
typedef struct {int clock_speed_hz,mode,spics_io_num,queue_size;} spi_device_interface_config_t;
typedef struct {unsigned flags,length;uint8_t tx_data[4];} spi_transaction_t;
esp_err_t spi_bus_initialize(int host,const spi_bus_config_t *c,int dma);
esp_err_t spi_bus_free(int host);
esp_err_t spi_bus_add_device(int host,const spi_device_interface_config_t *c,spi_device_handle_t *d);
esp_err_t spi_bus_remove_device(spi_device_handle_t d);
esp_err_t spi_device_queue_trans(spi_device_handle_t d,spi_transaction_t *t,unsigned ticks);
esp_err_t spi_device_get_trans_result(spi_device_handle_t d,spi_transaction_t **t,unsigned ticks);
#endif

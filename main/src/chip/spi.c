/*
 * spi.c
 *
 *  Created on: 2018-02-10 16:38
 *      Author: Jack Chen <redchenjs@live.com>
 */

#include "esp_log.h"

#include "driver/spi_master.h"

#include "board/st7789.h"

#define HSPI_TAG "hspi"

#ifdef CONFIG_ENABLE_GUI
spi_device_handle_t hspi;

void hspi_init(void)
{
    spi_bus_config_t bus_conf = {
        .miso_io_num = -1,
        .mosi_io_num = CONFIG_SPI_MOSI_PIN,
        .sclk_io_num = CONFIG_SPI_SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = ST7789_SCREEN_WIDTH * ST7789_SCREEN_HEIGHT * 2
    };
    ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &bus_conf, 1));

    spi_device_interface_config_t dev_conf = {
        .mode = 0,                                // SPI mode 0
        .spics_io_num = CONFIG_SPI_CS_PIN,        // CS pin
        .clock_speed_hz = 40000000,               // clock out at 40 MHz
        .pre_cb = st7789_setpin_dc,               // specify pre-transfer callback to handle D/C line
        .queue_size = 2,                          // we want to be able to queue 2 transactions at a time
        .flags = SPI_DEVICE_3WIRE | SPI_DEVICE_HALFDUPLEX
    };
    ESP_ERROR_CHECK(spi_bus_add_device(HSPI_HOST, &dev_conf, &hspi));

    ESP_LOGI(HSPI_TAG, "initialized, sclk: %d, mosi: %d, miso: %d, cs: %d",
             bus_conf.sclk_io_num,
             bus_conf.mosi_io_num,
             bus_conf.miso_io_num,
             dev_conf.spics_io_num
    );
}
#endif

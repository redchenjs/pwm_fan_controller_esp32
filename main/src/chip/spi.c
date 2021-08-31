/*
 * spi.c
 *
 *  Created on: 2018-02-10 16:38
 *      Author: Jack Chen <redchenjs@live.com>
 */

#include "esp_log.h"

#include "chip/spi.h"
#include "board/st7789.h"

#ifdef CONFIG_ENABLE_GUI
spi_device_handle_t spi_host;

void spi_host_init(void)
{
    spi_bus_config_t bus_conf = {
        .miso_io_num = -1,
        .mosi_io_num = CONFIG_SPI_MOSI_PIN,
        .sclk_io_num = CONFIG_SPI_SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = ST7789_SCREEN_WIDTH * ST7789_SCREEN_HEIGHT * 2
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI_HOST_NUM, &bus_conf, 1));

    spi_device_interface_config_t dev_conf = {
        .mode = 0,
        .spics_io_num = CONFIG_SPI_CS_PIN,
        .clock_speed_hz = SPI_MASTER_FREQ_40M,
        .pre_cb = st7789_setpin_dc,
        .queue_size = 2,
        .flags = SPI_DEVICE_NO_DUMMY
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI_HOST_NUM, &dev_conf, &spi_host));

    ESP_LOGI(SPI_HOST_TAG, "initialized, sclk: %d, mosi: %d, miso: %d, cs: %d",
             bus_conf.sclk_io_num,
             bus_conf.mosi_io_num,
             bus_conf.miso_io_num,
             dev_conf.spics_io_num
    );
}
#endif

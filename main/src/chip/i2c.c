/*
 * i2c.c
 *
 *  Created on: 2019-04-18 20:27
 *      Author: Jack Chen <redchenjs@live.com>
 */

#include "esp_log.h"

#include "chip/i2c.h"

#ifdef CONFIG_ENABLE_POWER_MONITOR
void i2c_host_init(void)
{
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = CONFIG_I2C_SDA_PIN,
        .scl_io_num = CONFIG_I2C_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_HOST_NUM, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_HOST_NUM, i2c_conf.mode, 0, 0, 0));
    ESP_ERROR_CHECK(i2c_set_timeout(I2C_HOST_NUM, 80 * (I2C_APB_CLK_FREQ / i2c_conf.master.clk_speed)));

    ESP_LOGI(I2C_HOST_TAG, "initialized, sda: %d, scl: %d", i2c_conf.sda_io_num, i2c_conf.scl_io_num);
}
#endif

/*
 * bt.c
 *
 *  Created on: 2018-03-09 10:36
 *      Author: Jack Chen <redchenjs@live.com>
 */

#include "esp_bt.h"
#include "esp_log.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"

#define TAG "bt"

const char *bt_dev_address = NULL;

void bt_init(void)
{
#ifdef CONFIG_ENABLE_BLE_CONTROL_IF
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    bt_dev_address = (const char *)esp_bt_dev_get_address();

    ESP_LOGI(TAG, "initialized, bt: 0, ble: 1");
#else
    ESP_ERROR_CHECK(esp_bt_mem_release(ESP_BT_MODE_BTDM));

    ESP_LOGI(TAG, "initialized, bt: 0, ble: 0");
#endif
}

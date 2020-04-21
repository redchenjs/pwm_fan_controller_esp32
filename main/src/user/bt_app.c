/*
 * bt_app.c
 *
 *  Created on: 2018-03-09 13:57
 *      Author: Jack Chen <redchenjs@live.com>
 */

#include "esp_log.h"
#include "esp_bt_main.h"
#include "esp_spp_api.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"

#include "core/os.h"
#include "user/bt_spp.h"

#define BT_APP_TAG "bt_app"
#define BT_GAP_TAG "bt_gap"

static void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_BT_GAP_AUTH_CMPL_EVT: {
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(BT_GAP_TAG, "authentication success: %s", param->auth_cmpl.device_name);
        } else {
            ESP_LOGE(BT_GAP_TAG, "authentication failed, status: %d", param->auth_cmpl.stat);
        }
        break;
    }
    default:
        break;
    }
}

void bt_app_init(void)
{
    xEventGroupSetBits(user_event_group, BT_SPP_IDLE_BIT);

    /* set up device name */
    esp_bt_dev_set_device_name(CONFIG_BT_NAME);

    /* register GAP callback */
    esp_bt_gap_register_callback(bt_app_gap_cb);

    esp_spp_register_callback(bt_app_spp_cb);
    esp_spp_init(ESP_SPP_MODE_CB);

    /*
     * Set default parameters for Legacy Pairing
     * Use fixed pin code
     */
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_FIXED;
    esp_bt_pin_code_t pin_code;
    pin_code[0] = '1';
    pin_code[1] = '2';
    pin_code[2] = '3';
    pin_code[3] = '4';
    esp_bt_gap_set_pin(pin_type, 4, pin_code);

    /* set discoverable and connectable mode, wait to be connected */
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

    ESP_LOGI(BT_APP_TAG, "started.");
}

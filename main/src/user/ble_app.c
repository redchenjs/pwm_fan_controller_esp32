/*
 * ble_app.c
 *
 *  Created on: 2019-07-04 22:50
 *      Author: Jack Chen <redchenjs@live.com>
 */

#include <string.h>

#include "esp_log.h"
#include "esp_gatts_api.h"
#include "esp_gap_bt_api.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"

#include "core/os.h"
#include "user/ble_gatts.h"

#define BLE_APP_TAG "ble_app"
#define BLE_GAP_TAG "ble_gap"

static uint8_t adv_config_done = 0;

#define adv_config_flag      (1 << 0)
#define scan_rsp_config_flag (1 << 1)

esp_ble_adv_params_t adv_params = {
    .adv_int_min       = 0x20,
    .adv_int_max       = 0x40,
    .adv_type          = ADV_TYPE_IND,
    .own_addr_type     = BLE_ADDR_TYPE_PUBLIC,
    .channel_map       = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

void ble_gap_init_adv_data(const char *name)
{
    int len = strlen(name);
    uint8_t raw_adv_data[len+5];
    // flag
    raw_adv_data[0] = 2;
    raw_adv_data[1] = ESP_BT_EIR_TYPE_FLAGS;
    raw_adv_data[2] = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
    // adv name
    raw_adv_data[3] = len + 1;
    raw_adv_data[4] = ESP_BLE_AD_TYPE_NAME_CMPL;
    for (int i=0; i<len; i++) {
        raw_adv_data[i+5] = *(name++);
    }
    // the length of adv data must be less than 31 bytes
    esp_err_t raw_adv_ret = esp_ble_gap_config_adv_data_raw(raw_adv_data, sizeof(raw_adv_data));
    if (raw_adv_ret) {
        ESP_LOGE(BLE_GAP_TAG, "config raw adv data failed, error code = 0x%x ", raw_adv_ret);
    }
    adv_config_done |= adv_config_flag;
    esp_err_t raw_scan_ret = esp_ble_gap_config_scan_rsp_data_raw(raw_adv_data, sizeof(raw_adv_data));
    if (raw_scan_ret) {
        ESP_LOGE(BLE_GAP_TAG, "config raw scan rsp data failed, error code = 0x%x", raw_scan_ret);
    }
    adv_config_done |= scan_rsp_config_flag;
}

static void ble_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        adv_config_done &= (~adv_config_flag);
        if (adv_config_done == 0) {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
        adv_config_done &= (~scan_rsp_config_flag);
        if (adv_config_done == 0) {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        // advertising start complete event to indicate advertising start successfully or failed
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(BLE_GAP_TAG, "advertising start failed");
        }
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(BLE_GAP_TAG, "advertising stop failed");
        }
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        break;
    case ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT:
        break;
    default:
        break;
    }
}

void ble_app_init(void)
{
    xEventGroupSetBits(user_event_group, BLE_GATTS_IDLE_BIT);

    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(ble_gatts_event_handler));
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(ble_gap_event_handler));

    ESP_ERROR_CHECK(esp_ble_gatts_app_register(PROFILE_IDX_FAN));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(PROFILE_IDX_VER));

    ESP_LOGI(BLE_APP_TAG, "started.");
}

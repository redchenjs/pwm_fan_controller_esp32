/*
 * ble_gatts.c
 *
 *  Created on: 2018-05-12 22:31
 *      Author: Jack Chen <redchenjs@live.com>
 */

#include <string.h>

#include "esp_log.h"
#include "esp_gatts_api.h"
#include "esp_gap_ble_api.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "core/os.h"
#include "core/app.h"
#include "user/fan.h"
#include "user/ble_app.h"
#include "user/ble_gatts.h"

#define BLE_GATTS_TAG "ble_gatts"

#define GATTS_FAN_TAG "gatts_fan"
#define GATTS_VER_TAG "gatts_ver"

#define GATTS_SRV_UUID_FAN      0x00AA
#define GATTS_CHAR_UUID_FAN     0xAA01
#define GATTS_DESC_UUID_FAN     0xDEAD
#define GATTS_NUM_HANDLE_FAN    4

#define GATTS_SRV_UUID_VER      0x00BB
#define GATTS_CHAR_UUID_VER     0xBB02
#define GATTS_DESC_UUID_VER     0xBEEF
#define GATTS_NUM_HANDLE_VER    4

#define PREPARE_BUF_MAX_SIZE    1024

typedef struct {
    uint8_t *prepare_buf;
    int      prepare_len;
} prepare_type_env_t;

static prepare_type_env_t fan_prepare_write_env;
static prepare_type_env_t ver_prepare_write_env;

static const char *s_gatts_conn_state_str[] = {"disconnected", "connected"};

static void profile_fan_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void profile_ver_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

/* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT */
gatts_profile_inst_t gatts_profile_tbl[PROFILE_IDX_MAX] = {
    [PROFILE_IDX_FAN] = { .gatts_cb = profile_fan_event_handler, .gatts_if = ESP_GATT_IF_NONE },
    [PROFILE_IDX_VER] = { .gatts_cb = profile_ver_event_handler, .gatts_if = ESP_GATT_IF_NONE },
};

static void gatts_write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param)
{
    esp_gatt_status_t status = ESP_GATT_OK;
    if (param->write.need_rsp) {
        if (param->write.is_prep) {
            if (prepare_write_env->prepare_buf == NULL) {
                prepare_write_env->prepare_buf = (uint8_t *)malloc(PREPARE_BUF_MAX_SIZE * sizeof(uint8_t));
                prepare_write_env->prepare_len = 0;
                if (prepare_write_env->prepare_buf == NULL) {
                    ESP_LOGE(BLE_GATTS_TAG, "malloc write buffer failed");
                    status = ESP_GATT_NO_RESOURCES;
                }
            } else {
                if(param->write.offset > PREPARE_BUF_MAX_SIZE) {
                    status = ESP_GATT_INVALID_OFFSET;
                } else if ((param->write.offset + param->write.len) > PREPARE_BUF_MAX_SIZE) {
                    status = ESP_GATT_INVALID_ATTR_LEN;
                }
            }

            esp_gatt_rsp_t *gatt_rsp = (esp_gatt_rsp_t *)malloc(sizeof(esp_gatt_rsp_t));
            gatt_rsp->attr_value.len = param->write.len;
            gatt_rsp->attr_value.handle = param->write.handle;
            gatt_rsp->attr_value.offset = param->write.offset;
            gatt_rsp->attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;

            memcpy(gatt_rsp->attr_value.value, param->write.value, param->write.len);

            esp_err_t response_err = esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, gatt_rsp);
            if (response_err != ESP_OK) {
               ESP_LOGE(BLE_GATTS_TAG, "send response error");
            }

            free(gatt_rsp);

            if (status != ESP_GATT_OK) {
                return;
            }

            memcpy(prepare_write_env->prepare_buf + param->write.offset,
                   param->write.value,
                   param->write.len);

            prepare_write_env->prepare_len += param->write.len;
        } else {
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, NULL);
        }
    }
}

static void gatts_exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param)
{
    if (prepare_write_env->prepare_buf) {
        free(prepare_write_env->prepare_buf);
        prepare_write_env->prepare_buf = NULL;
    }
    prepare_write_env->prepare_len = 0;
}

static void profile_fan_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT:
        ble_gap_init_adv_data(CONFIG_BT_NAME);

        gatts_profile_tbl[PROFILE_IDX_FAN].service_id.is_primary = true;
        gatts_profile_tbl[PROFILE_IDX_FAN].service_id.id.inst_id = 0x00;
        gatts_profile_tbl[PROFILE_IDX_FAN].service_id.id.uuid.len = ESP_UUID_LEN_16;
        gatts_profile_tbl[PROFILE_IDX_FAN].service_id.id.uuid.uuid.uuid16 = GATTS_SRV_UUID_FAN;

        esp_ble_gatts_create_service(gatts_if, &gatts_profile_tbl[PROFILE_IDX_FAN].service_id, GATTS_NUM_HANDLE_FAN);
        break;
    case ESP_GATTS_READ_EVT: {
        esp_gatt_rsp_t rsp;

        memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
        rsp.attr_value.handle = param->read.handle;
        rsp.attr_value.len = 8;

        rsp.attr_value.value[0] = 0b0010;
        rsp.attr_value.value[1] = 0x00;
        rsp.attr_value.value[2] = 0x00;
        rsp.attr_value.value[3] = 0x00;
        rsp.attr_value.value[4] = 0x00;
        rsp.attr_value.value[5] = 0x00;
        rsp.attr_value.value[6] = fan_get_duty();
        rsp.attr_value.value[7] = 0x00;

        esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
        break;
    }
    case ESP_GATTS_WRITE_EVT: {
        if (!param->write.is_prep) {
            switch (param->write.value[0]) {
            case 0xEF: {
                if (param->write.len == 1) {    // Restore Default Configuration
                    fan_set_duty(0x00);
                } else if (param->write.len == 8) { // Update with New Configuration
                    fan_set_duty(param->write.value[6]);
                } else {
                    ESP_LOGE(GATTS_FAN_TAG, "command 0x%02X error", param->write.value[0]);
                }
                break;
            }
            default:
                ESP_LOGW(GATTS_FAN_TAG, "unknown command: 0x%02X", param->write.value[0]);
                break;
            }
        }

        gatts_write_event_env(gatts_if, &fan_prepare_write_env, param);
        break;
    }
    case ESP_GATTS_EXEC_WRITE_EVT:
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
        gatts_exec_write_event_env(&fan_prepare_write_env, param);
        break;
    case ESP_GATTS_MTU_EVT:
        break;
    case ESP_GATTS_UNREG_EVT:
        break;
    case ESP_GATTS_CREATE_EVT:
        gatts_profile_tbl[PROFILE_IDX_FAN].service_handle = param->create.service_handle;
        gatts_profile_tbl[PROFILE_IDX_FAN].char_uuid.len = ESP_UUID_LEN_16;
        gatts_profile_tbl[PROFILE_IDX_FAN].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_FAN;

        esp_ble_gatts_start_service(gatts_profile_tbl[PROFILE_IDX_FAN].service_handle);

        esp_err_t add_char_ret = esp_ble_gatts_add_char(gatts_profile_tbl[PROFILE_IDX_FAN].service_handle,
                                                        &gatts_profile_tbl[PROFILE_IDX_FAN].char_uuid,
                                                        ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                        ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE,
                                                        NULL,
                                                        NULL);
        if (add_char_ret) {
            ESP_LOGE(GATTS_FAN_TAG, "add char failed, error code =%x", add_char_ret);
        }
        break;
    case ESP_GATTS_ADD_INCL_SRVC_EVT:
        break;
    case ESP_GATTS_ADD_CHAR_EVT:
        gatts_profile_tbl[PROFILE_IDX_FAN].char_handle = param->add_char.attr_handle;
        gatts_profile_tbl[PROFILE_IDX_FAN].descr_uuid.len = ESP_UUID_LEN_16;
        gatts_profile_tbl[PROFILE_IDX_FAN].descr_uuid.uuid.uuid16 = GATTS_DESC_UUID_FAN;

        esp_err_t add_descr_ret = esp_ble_gatts_add_char_descr(gatts_profile_tbl[PROFILE_IDX_FAN].service_handle,
                                                               &gatts_profile_tbl[PROFILE_IDX_FAN].descr_uuid,
                                                               ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                               NULL,
                                                               NULL);
        if (add_descr_ret) {
            ESP_LOGE(GATTS_FAN_TAG, "add char descr failed, error code =%x", add_descr_ret);
        }
        break;
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        gatts_profile_tbl[PROFILE_IDX_FAN].descr_handle = param->add_char_descr.attr_handle;
        break;
    case ESP_GATTS_DELETE_EVT:
        break;
    case ESP_GATTS_START_EVT:
        break;
    case ESP_GATTS_STOP_EVT:
        break;
    case ESP_GATTS_CONNECT_EVT: {
        xEventGroupClearBits(user_event_group, BLE_GATTS_IDLE_BIT);

        uint8_t *bda = param->connect.remote_bda;
        ESP_LOGI(GATTS_FAN_TAG, "GATTS connection state: %s, [%02x:%02x:%02x:%02x:%02x:%02x]",
                 s_gatts_conn_state_str[1], bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);

        gatts_profile_tbl[PROFILE_IDX_FAN].conn_id = param->connect.conn_id;

        break;
    }
    case ESP_GATTS_DISCONNECT_EVT: {
        uint8_t *bda = param->connect.remote_bda;
        ESP_LOGI(GATTS_FAN_TAG, "GATTS connection state: %s, [%02x:%02x:%02x:%02x:%02x:%02x]",
                 s_gatts_conn_state_str[0], bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);

        EventBits_t uxBits = xEventGroupGetBits(user_event_group);
        if (!(uxBits & OS_PWR_RESTART_BIT)) {
            esp_ble_gap_start_advertising(&adv_params);
        }

        xEventGroupSetBits(user_event_group, BLE_GATTS_IDLE_BIT);

        break;
    }
    case ESP_GATTS_CONF_EVT:
    case ESP_GATTS_OPEN_EVT:
    case ESP_GATTS_CANCEL_OPEN_EVT:
    case ESP_GATTS_CLOSE_EVT:
    case ESP_GATTS_LISTEN_EVT:
    case ESP_GATTS_CONGEST_EVT:
    default:
        break;
    }
}

static void profile_ver_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT:
        gatts_profile_tbl[PROFILE_IDX_VER].service_id.is_primary = true;
        gatts_profile_tbl[PROFILE_IDX_VER].service_id.id.inst_id = 0x00;
        gatts_profile_tbl[PROFILE_IDX_VER].service_id.id.uuid.len = ESP_UUID_LEN_16;
        gatts_profile_tbl[PROFILE_IDX_VER].service_id.id.uuid.uuid.uuid16 = GATTS_SRV_UUID_VER;

        esp_ble_gatts_create_service(gatts_if, &gatts_profile_tbl[PROFILE_IDX_VER].service_id, GATTS_NUM_HANDLE_VER);
        break;
    case ESP_GATTS_READ_EVT: {
        esp_gatt_rsp_t rsp;

        memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
        rsp.attr_value.handle = param->read.handle;
        rsp.attr_value.len = strlen(app_get_version()) > 20 ? 20 : strlen(app_get_version());
        memcpy(rsp.attr_value.value, app_get_version(), rsp.attr_value.len);

        esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
        break;
    }
    case ESP_GATTS_WRITE_EVT:
        gatts_write_event_env(gatts_if, &ver_prepare_write_env, param);
        break;
    case ESP_GATTS_EXEC_WRITE_EVT:
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
        gatts_exec_write_event_env(&ver_prepare_write_env, param);
        break;
    case ESP_GATTS_MTU_EVT:
        break;
    case ESP_GATTS_UNREG_EVT:
        break;
    case ESP_GATTS_CREATE_EVT:
        gatts_profile_tbl[PROFILE_IDX_VER].service_handle = param->create.service_handle;
        gatts_profile_tbl[PROFILE_IDX_VER].char_uuid.len = ESP_UUID_LEN_16;
        gatts_profile_tbl[PROFILE_IDX_VER].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_VER;

        esp_ble_gatts_start_service(gatts_profile_tbl[PROFILE_IDX_VER].service_handle);

        esp_err_t add_char_ret = esp_ble_gatts_add_char(gatts_profile_tbl[PROFILE_IDX_VER].service_handle,
                                                        &gatts_profile_tbl[PROFILE_IDX_VER].char_uuid,
                                                        ESP_GATT_PERM_READ,
                                                        ESP_GATT_CHAR_PROP_BIT_READ,
                                                        NULL,
                                                        NULL);
        if (add_char_ret) {
            ESP_LOGE(GATTS_VER_TAG, "add char failed, error code =%x", add_char_ret);
        }
        break;
    case ESP_GATTS_ADD_INCL_SRVC_EVT:
        break;
    case ESP_GATTS_ADD_CHAR_EVT:
        gatts_profile_tbl[PROFILE_IDX_VER].char_handle = param->add_char.attr_handle;
        gatts_profile_tbl[PROFILE_IDX_VER].descr_uuid.len = ESP_UUID_LEN_16;
        gatts_profile_tbl[PROFILE_IDX_VER].descr_uuid.uuid.uuid16 = GATTS_DESC_UUID_VER;

        esp_err_t add_descr_ret = esp_ble_gatts_add_char_descr(gatts_profile_tbl[PROFILE_IDX_VER].service_handle,
                                                               &gatts_profile_tbl[PROFILE_IDX_VER].descr_uuid,
                                                               ESP_GATT_PERM_READ,
                                                               NULL,
                                                               NULL);
        if (add_descr_ret) {
            ESP_LOGE(GATTS_VER_TAG, "add char descr failed, error code =%x", add_descr_ret);
        }
        break;
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        gatts_profile_tbl[PROFILE_IDX_VER].descr_handle = param->add_char_descr.attr_handle;
        break;
    case ESP_GATTS_DELETE_EVT:
        break;
    case ESP_GATTS_START_EVT:
        break;
    case ESP_GATTS_STOP_EVT:
        break;
    case ESP_GATTS_CONNECT_EVT: {
        uint8_t *bda = param->connect.remote_bda;
        ESP_LOGI(GATTS_VER_TAG, "GATTS connection state: %s, [%02x:%02x:%02x:%02x:%02x:%02x]",
                 s_gatts_conn_state_str[1], bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);

        gatts_profile_tbl[PROFILE_IDX_VER].conn_id = param->connect.conn_id;

        break;
    }
    case ESP_GATTS_DISCONNECT_EVT: {
        uint8_t *bda = param->connect.remote_bda;
        ESP_LOGI(GATTS_VER_TAG, "GATTS connection state: %s, [%02x:%02x:%02x:%02x:%02x:%02x]",
                 s_gatts_conn_state_str[0], bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
        break;
    }
    case ESP_GATTS_CONF_EVT:
    case ESP_GATTS_OPEN_EVT:
    case ESP_GATTS_CANCEL_OPEN_EVT:
    case ESP_GATTS_CLOSE_EVT:
    case ESP_GATTS_LISTEN_EVT:
    case ESP_GATTS_CONGEST_EVT:
    default:
        break;
    }
}

void ble_gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gatts_profile_tbl[param->reg.app_id].gatts_if = gatts_if;
        } else {
            ESP_LOGE(BLE_GATTS_TAG, "reg app failed, app_id %04x, status %d",
                     param->reg.app_id,
                     param->reg.status);
            return;
        }
    }

    /* If the gatts_if equal to profile A, call profile A cb handler,
     * so here call each profile's callback */
    for (int idx=0; idx<PROFILE_IDX_MAX; idx++) {
        if (gatts_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
            gatts_if == gatts_profile_tbl[idx].gatts_if) {
            if (gatts_profile_tbl[idx].gatts_cb) {
                gatts_profile_tbl[idx].gatts_cb(event, gatts_if, param);
            }
        }
    }
}

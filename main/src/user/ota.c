/*
 * ota.c
 *
 *  Created on: 2020-02-12 15:48
 *      Author: Jack Chen <redchenjs@live.com>
 */

#include <string.h>

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_spp_api.h"

#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"

#include "core/os.h"
#include "core/app.h"
#include "user/fan.h"
#include "user/gui.h"
#include "user/bt_app.h"
#include "user/bt_spp.h"
#include "user/ble_app.h"
#include "user/ble_gatts.h"

#define OTA_TAG "ota"

#define ota_send_response(X) \
    esp_spp_write(spp_conn_handle, strlen(rsp_str[X]), (uint8_t *)rsp_str[X])

#define ota_send_data(X, N) \
    esp_spp_write(spp_conn_handle, N, (uint8_t *)X)

#define CMD_FMT_UPD "FW+UPD:%u"
#define CMD_FMT_RST "FW+RST!"
#define CMD_FMT_RAM "FW+RAM?"
#define CMD_FMT_VER "FW+VER?"

enum cmd_idx {
    CMD_IDX_UPD = 0x0,
    CMD_IDX_RST = 0x1,
    CMD_IDX_RAM = 0x2,
    CMD_IDX_VER = 0x3,
};

typedef struct {
    const char prefix;
    const char format[32];
} cmd_fmt_t;

static const cmd_fmt_t cmd_fmt[] = {
    { .prefix = 7, .format = CMD_FMT_UPD"\r\n" },   // Firmware Update
    { .prefix = 7, .format = CMD_FMT_RST"\r\n" },   // Chip Reset
    { .prefix = 7, .format = CMD_FMT_RAM"\r\n" },   // RAM Infomation
    { .prefix = 7, .format = CMD_FMT_VER"\r\n" },   // Firmware Version
};

enum rsp_idx {
    RSP_IDX_OK    = 0x0,
    RSP_IDX_FAIL  = 0x1,
    RSP_IDX_DONE  = 0x2,
    RSP_IDX_ERROR = 0x3,
};

static const char rsp_str[][32] = {
    "OK\r\n",           // OK
    "FAIL\r\n",         // Fail
    "DONE\r\n",         // Done
    "ERROR\r\n",        // Error
};

static bool data_err = false;
static bool data_recv = false;
static uint32_t data_length = 0;

static RingbufHandle_t ota_buff = NULL;

static const esp_partition_t *update_partition = NULL;
static esp_ota_handle_t update_handle = 0;

static int ota_parse_command(esp_spp_cb_param_t *param)
{
    for (int i=0; i<sizeof(cmd_fmt)/sizeof(cmd_fmt_t); i++) {
        if (strncmp(cmd_fmt[i].format, (const char *)param->data_ind.data, cmd_fmt[i].prefix) == 0) {
            return i;
        }
    }
    return -1;
}

static void ota_write_task(void *pvParameter)
{
    uint8_t *data = NULL;
    uint32_t size = 0;
    esp_err_t err = ESP_OK;

    ESP_LOGI(OTA_TAG, "write started.");

    while (data_length > 0) {
        if (!data_recv) {
            ESP_LOGE(OTA_TAG, "write aborted.");

            goto write_fail;
        }

        if (data_length >= 990) {
            data = (uint8_t *)xRingbufferReceiveUpTo(ota_buff, &size, 10 / portTICK_RATE_MS, 990);
        } else {
            data = (uint8_t *)xRingbufferReceiveUpTo(ota_buff, &size, 10 / portTICK_RATE_MS, data_length);
        }

        if (data == NULL || size == 0) {
            continue;
        }

        err = esp_ota_write(update_handle, (const void *)data, size);
        if (err != ESP_OK) {
            ESP_LOGE(OTA_TAG, "write failed.");

            data_err = true;

            ota_send_response(RSP_IDX_FAIL);

            goto write_fail;
        }

        data_length -= size;

        if (data_length == 0) {
            err = esp_ota_end(update_handle);
            if (err != ESP_OK) {
                ESP_LOGE(OTA_TAG, "image data error.");

                data_err = true;

                ota_send_response(RSP_IDX_FAIL);

                goto write_fail;
            }

            err = esp_ota_set_boot_partition(update_partition);
            if (err != ESP_OK) {
                ESP_LOGE(OTA_TAG, "set boot partition failed.");

                data_err = true;

                ota_send_response(RSP_IDX_FAIL);

                goto write_fail;
            }
        }

        vRingbufferReturnItem(ota_buff, (void *)data);
    }

    ESP_LOGI(OTA_TAG, "write done.");

    ota_send_response(RSP_IDX_DONE);

write_fail:
    vRingbufferDelete(ota_buff);
    ota_buff = NULL;

    data_recv = false;

    vTaskDelete(NULL);
}

void ota_exec(esp_spp_cb_param_t *param)
{
    if (data_err) {
        return;
    }

    if (!data_recv) {
        int cmd_idx = ota_parse_command(param);

        switch (cmd_idx) {
            case CMD_IDX_UPD: {
                data_length = 0;
                sscanf((const char *)param->data_ind.data, CMD_FMT_UPD, &data_length);
                ESP_LOGI(OTA_TAG, "GET command: "CMD_FMT_UPD, data_length);

                EventBits_t uxBits = xEventGroupGetBits(user_event_group);
                if (data_length != 0 && !(uxBits & BT_OTA_LOCK_BIT)
#ifdef CONFIG_ENABLE_BLE_CONTROL_IF
                    && (uxBits & BLE_GATTS_IDLE_BIT)
#endif
                ) {
                    if (!update_handle) {
#ifdef CONFIG_ENABLE_GUI
                        gui_set_mode(0);
#endif
                        fan_set_mode(0);

#ifdef CONFIG_ENABLE_BLE_CONTROL_IF
                        esp_ble_gap_stop_advertising();
#endif
                    }

                    update_partition = esp_ota_get_next_update_partition(NULL);
                    if (update_partition != NULL) {
                        ESP_LOGI(OTA_TAG, "writing to partition subtype %d at offset 0x%x",
                                 update_partition->subtype, update_partition->address);
                    } else {
                        ESP_LOGE(OTA_TAG, "no ota partition to write");

                        ota_send_response(RSP_IDX_ERROR);

                        break;
                    }

                    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
                    if (err != ESP_OK) {
                        ESP_LOGE(OTA_TAG, "failed to start ota");

                        ota_send_response(RSP_IDX_ERROR);

                        break;
                    }

                    ota_buff = xRingbufferCreate(990, RINGBUF_TYPE_BYTEBUF);
                    if (!ota_buff) {
                        ota_send_response(RSP_IDX_ERROR);
                    } else {
                        data_recv = true;

                        ota_send_response(RSP_IDX_OK);

                        xTaskCreatePinnedToCore(ota_write_task, "otaWriteT", 1920, NULL, 9, NULL, 1);
                    }
                } else if ((uxBits & BT_OTA_LOCK_BIT)
#ifdef CONFIG_ENABLE_BLE_CONTROL_IF
                        || !(uxBits & BLE_GATTS_IDLE_BIT)
#endif
                ) {
                    ota_send_response(RSP_IDX_FAIL);
                } else {
                    ota_send_response(RSP_IDX_ERROR);
                }

                break;
            }
            case CMD_IDX_RST: {
                ESP_LOGI(OTA_TAG, "GET command: "CMD_FMT_RST);

                xEventGroupSetBits(user_event_group, BT_OTA_LOCK_BIT);

                EventBits_t uxBits = xEventGroupGetBits(user_event_group);
#ifdef CONFIG_ENABLE_BLE_CONTROL_IF
                if (!(uxBits & BLE_GATTS_IDLE_BIT)) {
                    esp_ble_gatts_close(gatts_profile_tbl[0].gatts_if, gatts_profile_tbl[0].conn_id);
                }
                os_power_restart_wait(BT_SPP_IDLE_BIT | BLE_GATTS_IDLE_BIT);
#else
                os_power_restart_wait(BT_SPP_IDLE_BIT);
#endif

                update_handle = 0;

                esp_spp_disconnect(param->write.handle);

                break;
            }
            case CMD_IDX_RAM: {
                ESP_LOGI(OTA_TAG, "GET command: "CMD_FMT_RAM);

                char rsp_str[40] = {0};
                ESP_LOGI(OTA_TAG, "free memory: %u bytes", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
                snprintf(rsp_str, sizeof(rsp_str), "%u\r\n", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));

                ota_send_data(rsp_str, strlen(rsp_str));

                break;
            }
            case CMD_IDX_VER: {
                ESP_LOGI(OTA_TAG, "GET command: "CMD_FMT_VER);

                char rsp_str[40] = {0};
                ESP_LOGI(OTA_TAG, "app version: %s", app_get_version());
                snprintf(rsp_str, sizeof(rsp_str), "%s\r\n", app_get_version());

                ota_send_data(rsp_str, strlen(rsp_str));

                break;
            }
            default:
                ESP_LOGW(OTA_TAG, "unknown command.");

                ota_send_response(RSP_IDX_ERROR);

                break;
        }
    } else {
        if (ota_buff) {
            xRingbufferSend(ota_buff, (void *)param->data_ind.data, param->data_ind.len, portMAX_DELAY);
        }
    }
}

void ota_end(void)
{
    data_err = false;
    data_recv = false;

    if (update_handle) {
        esp_ota_end(update_handle);
        update_handle = 0;

        data_length = 0;

#ifdef CONFIG_ENABLE_BLE_CONTROL_IF
        esp_ble_gap_start_advertising(&adv_params);
#endif

        fan_set_mode(1);
#ifdef CONFIG_ENABLE_GUI
        gui_set_mode(1);
#endif
    }
}

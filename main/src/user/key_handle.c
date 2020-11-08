/*
 * key_handle.c
 *
 *  Created on: 2019-07-06 10:35
 *      Author: Jack Chen <redchenjs@live.com>
 */

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "core/os.h"
#include "core/app.h"

#include "user/pwr.h"
#include "user/fan.h"
#include "user/key.h"
#include "user/led.h"
#include "user/gui.h"
#include "user/ble_gatts.h"

#ifdef CONFIG_ENABLE_POWER_MODE_KEY
void power_mode_key_handle(void)
{
    key_set_scan_mode(KEY_SCAN_MODE_IDX_OFF);

    pwr_set_mode((pwr_get_mode() + 1) % PWR_MODE_IDX_MAX);
    vTaskDelay(200 / portTICK_RATE_MS);

    key_set_scan_mode(KEY_SCAN_MODE_IDX_ON);
}
#endif

#ifdef CONFIG_ENABLE_SLEEP_KEY
void sleep_key_handle(void)
{
    key_set_scan_mode(KEY_SCAN_MODE_IDX_OFF);

#ifdef CONFIG_ENABLE_GUI
    gui_set_mode(GUI_MODE_IDX_OFF);
#endif
    fan_set_mode(FAN_MODE_IDX_OFF);

#ifdef CONFIG_ENABLE_QC
    pwr_deinit();
#endif

#ifdef CONFIG_ENABLE_BLE_CONTROL_IF
    EventBits_t uxBits = xEventGroupGetBits(user_event_group);
    if (!(uxBits & BLE_GATTS_IDLE_BIT)) {
        esp_ble_gatts_close(gatts_profile_tbl[PROFILE_IDX_OTA].gatts_if,
                            gatts_profile_tbl[PROFILE_IDX_OTA].conn_id);
    }
    os_pwr_sleep_wait(BLE_GATTS_IDLE_BIT);
#else
    vTaskDelay(500 / portTICK_RATE_MS);

    os_pwr_sleep_wait(OS_PWR_DUMMY_BIT);
#endif
}
#endif

/*
 * key_handle.c
 *
 *  Created on: 2019-07-06 10:35
 *      Author: Jack Chen <redchenjs@live.com>
 */

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/dac.h"
#include "driver/rtc_io.h"

#include "core/os.h"
#include "core/app.h"

#include "user/pwr.h"
#include "user/fan.h"
#include "user/led.h"
#include "user/gui.h"
#include "user/ble_gatts.h"

#ifdef CONFIG_ENABLE_POWER_MODE_KEY
void power_mode_key_handle(void)
{
    pwr_set_mode((pwr_get_mode() + 1) % PWR_IDX_MAX);

    vTaskDelay(200 / portTICK_RATE_MS);

    xEventGroupSetBits(user_event_group, KEY_RUN_BIT);
}
#endif

#ifdef CONFIG_ENABLE_SLEEP_KEY
void sleep_key_handle(void)
{
#ifdef CONFIG_ENABLE_LED
    led_set_mode(7);
#endif
#ifdef CONFIG_ENABLE_GUI
    gui_set_mode(0);
#endif
    fan_set_mode(0);

#ifdef CONFIG_ENABLE_QC
    dac_output_disable(DAC_CHANNEL_1);
    dac_output_disable(DAC_CHANNEL_2);
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

    os_pwr_sleep_wait(0);
#endif
}
#endif

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

#define PWR_KEY_TAG "pwr_key"
#define SLP_KEY_TAG "slp_key"

#ifdef CONFIG_ENABLE_PWR_KEY
void pwr_key_handle(void)
{
    pwr_set_mode((pwr_get_mode() + 1) % PWR_IDX_MAX);

    vTaskDelay(1000 / portTICK_RATE_MS);

    xEventGroupSetBits(user_event_group, KEY_RUN_BIT);
}
#endif

#ifdef CONFIG_ENABLE_SLP_KEY
void slp_key_handle(void)
{
#ifdef CONFIG_ENABLE_LED
    led_set_mode(7);
#endif
#ifdef CONFIG_ENABLE_GUI
    gui_set_mode(0);
#endif
    fan_set_mode(0);

    dac_output_disable(DAC_CHANNEL_1);
    dac_output_disable(DAC_CHANNEL_2);

#ifdef CONFIG_ENABLE_BLE_IF
    EventBits_t uxBits = xEventGroupGetBits(user_event_group);
    if (!(uxBits & BLE_GATTS_IDLE_BIT)) {
        esp_ble_gatts_close(gatts_profile_tbl[PROFILE_IDX_OTA].gatts_if,
                            gatts_profile_tbl[PROFILE_IDX_OTA].conn_id);
    }
#endif

    os_pwr_slp_wait(BLE_GATTS_IDLE_BIT);
}
#endif

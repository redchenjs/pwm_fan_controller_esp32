/*
 * os.c
 *
 *  Created on: 2018-03-04 20:07
 *      Author: Jack Chen <redchenjs@live.com>
 */

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "core/os.h"

#define OS_PWR_TAG "os_power"

EventGroupHandle_t user_event_group;

#if defined(CONFIG_ENABLE_WAKEUP_KEY) || defined(CONFIG_ENABLE_SLEEP_KEY) || defined(CONFIG_ENABLE_OTA_OVER_SPP)
static EventBits_t restart_wait_bits = 0;

static void os_power_task_handle(void *pvParameters)
{
    ESP_LOGI(OS_PWR_TAG, "started.");

    while (1) {
        xEventGroupWaitBits(
            user_event_group,
            OS_PWR_RESTART_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY
        );

        EventBits_t uxBits = xEventGroupGetBits(user_event_group);
        if (uxBits & OS_PWR_RESTART_BIT) {
            for (int i=3; i>0; i--) {
                ESP_LOGW(OS_PWR_TAG, "restarting in %ds", i);
                vTaskDelay(1000 / portTICK_RATE_MS);
            }

            xEventGroupWaitBits(
                user_event_group,
                restart_wait_bits,
                pdFALSE,
                pdTRUE,
                portMAX_DELAY
            );

            ESP_LOGW(OS_PWR_TAG, "restart now");
            esp_restart();
        }
    }
}

void os_power_restart_wait(EventBits_t bits)
{
    if (bits) {
        restart_wait_bits = bits;
        xEventGroupSetBits(user_event_group, OS_PWR_RESTART_BIT);
    } else {
        ESP_LOGW(OS_PWR_TAG, "restart now");
        esp_restart();
    }
}
#endif

void os_init(void)
{
    user_event_group = xEventGroupCreate();

#ifdef CONFIG_ENABLE_OTA_OVER_SPP
    xTaskCreatePinnedToCore(os_power_task_handle, "osPowerT", 2048, NULL, 5, NULL, 0);
#endif
}

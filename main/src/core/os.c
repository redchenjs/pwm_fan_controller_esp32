/*
 * os.c
 *
 *  Created on: 2018-03-04 20:07
 *      Author: Jack Chen <redchenjs@live.com>
 */

#include "esp_log.h"
#include "esp_sleep.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "driver/rtc_io.h"

#include "core/os.h"

#define OS_PWR_TAG "os_pwr"

EventGroupHandle_t user_event_group;

#if defined(CONFIG_ENABLE_SLP_KEY) || defined(CONFIG_ENABLE_BLE_IF)
static EventBits_t slp_wait_bits = 0;
static EventBits_t rst_wait_bits = 0;

static void os_pwr_task_handle(void *pvParameters)
{
    ESP_LOGI(OS_PWR_TAG, "started.");

    while (1) {
        xEventGroupWaitBits(
            user_event_group,
            OS_PWR_SLP_BIT | OS_PWR_RST_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY
        );

        EventBits_t uxBits = xEventGroupGetBits(user_event_group);
        if (uxBits & OS_PWR_SLP_BIT) {
            for (int i=3; i>0; i--) {
                ESP_LOGW(OS_PWR_TAG, "sleeping in %ds", i);
                vTaskDelay(1000 / portTICK_RATE_MS);
            }

            xEventGroupWaitBits(
                user_event_group,
                slp_wait_bits,
                pdFALSE,
                pdTRUE,
                portMAX_DELAY
            );

#ifdef CONFIG_ENABLE_SLP_KEY
            esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
            rtc_gpio_set_direction(CONFIG_SLP_KEY_PIN, RTC_GPIO_MODE_INPUT_ONLY);
    #ifdef CONFIG_SLP_KEY_ACTIVE_LOW
            rtc_gpio_pulldown_dis(CONFIG_SLP_KEY_PIN);
            rtc_gpio_pullup_en(CONFIG_SLP_KEY_PIN);
            esp_sleep_enable_ext1_wakeup(1ULL << CONFIG_SLP_KEY_PIN, ESP_EXT1_WAKEUP_ALL_LOW);
    #else
            rtc_gpio_pullup_dis(CONFIG_SLP_KEY_PIN);
            rtc_gpio_pulldown_en(CONFIG_SLP_KEY_PIN);
            esp_sleep_enable_ext1_wakeup(1ULL << CONFIG_SLP_KEY_PIN, ESP_EXT1_WAKEUP_ANY_HIGH);
    #endif
#endif

            ESP_LOGW(OS_PWR_TAG, "sleep now");
            esp_deep_sleep_start();
        } else if (uxBits & OS_PWR_RST_BIT) {
            for (int i=3; i>0; i--) {
                ESP_LOGW(OS_PWR_TAG, "restarting in %ds", i);
                vTaskDelay(1000 / portTICK_RATE_MS);
            }

            xEventGroupWaitBits(
                user_event_group,
                rst_wait_bits,
                pdFALSE,
                pdTRUE,
                portMAX_DELAY
            );

            ESP_LOGW(OS_PWR_TAG, "restart now");
            esp_restart();
        }
    }
}

void os_pwr_slp_wait(EventBits_t bits)
{
    if (bits) {
        slp_wait_bits = bits;
        xEventGroupSetBits(user_event_group, OS_PWR_SLP_BIT);
    } else {
#ifdef CONFIG_ENABLE_SLP_KEY
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
        rtc_gpio_set_direction(CONFIG_SLP_KEY_PIN, RTC_GPIO_MODE_INPUT_ONLY);
    #ifdef CONFIG_ENABLE_SLP_KEY_LOW
        rtc_gpio_pulldown_dis(CONFIG_SLP_KEY_PIN);
        rtc_gpio_pullup_en(CONFIG_SLP_KEY_PIN);
        esp_sleep_enable_ext1_wakeup(1ULL << CONFIG_SLP_KEY_PIN, ESP_EXT1_WAKEUP_ALL_LOW);
    #else
        rtc_gpio_pullup_dis(CONFIG_SLP_KEY_PIN);
        rtc_gpio_pulldown_en(CONFIG_SLP_KEY_PIN);
        esp_sleep_enable_ext1_wakeup(1ULL << CONFIG_SLP_KEY_PIN, ESP_EXT1_WAKEUP_ANY_HIGH);
    #endif
#endif
        ESP_LOGW(OS_PWR_TAG, "sleep now");
        esp_deep_sleep_start();
    }
}

void os_pwr_rst_wait(EventBits_t bits)
{
    if (bits) {
        rst_wait_bits = bits;
        xEventGroupSetBits(user_event_group, OS_PWR_RST_BIT);
    } else {
        ESP_LOGW(OS_PWR_TAG, "restart now");
        esp_restart();
    }
}
#endif

void os_init(void)
{
    user_event_group = xEventGroupCreate();

#if defined(CONFIG_ENABLE_SLP_KEY) || defined(CONFIG_ENABLE_BLE_IF)
    xTaskCreatePinnedToCore(os_pwr_task_handle, "osPwrT", 2048, NULL, 5, NULL, 0);
#endif
}

/*
 * ec.c
 *
 *  Created on: 2020-05-25 13:32
 *      Author: Jack Chen <redchenjs@live.com>
 */

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "driver/gpio.h"

#include "core/os.h"
#include "user/ec.h"
#include "user/fan.h"

#define TAG "ec"

#ifdef CONFIG_ENABLE_EC
static void pin_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = BIT64(CONFIG_EC_PHASE_A_PIN) |
                        BIT64(CONFIG_EC_PHASE_B_PIN) |
                        BIT64(CONFIG_EC_BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = true,
        .pull_down_en = false,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
}

static void ec_task(void *pvParameter)
{
    portTickType xLastWakeTime;
    bool phase_a_p = false;
    bool phase_a_n = false;
    bool phase_b_p = false;
    bool phase_b_n = false;
    bool button_p  = false;
    bool button_n  = false;
    uint32_t fan_evt = 0;

    pin_init();

    ESP_LOGI(TAG, "started.");

    phase_a_n = gpio_get_level(CONFIG_EC_PHASE_A_PIN);
    phase_b_n = gpio_get_level(CONFIG_EC_PHASE_B_PIN);
    button_n  = gpio_get_level(CONFIG_EC_BUTTON_PIN);

    while (1) {
        xEventGroupWaitBits(
            user_event_group,
            FAN_RUN_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY
        );

        xLastWakeTime = xTaskGetTickCount();

        fan_evt = EC_EVT_N;

        phase_a_p = phase_a_n;
        phase_b_p = phase_b_n;
        button_p  = button_n;

        phase_a_n = gpio_get_level(CONFIG_EC_PHASE_A_PIN);
        phase_b_n = gpio_get_level(CONFIG_EC_PHASE_B_PIN);
        button_n  = gpio_get_level(CONFIG_EC_BUTTON_PIN);

        if (phase_a_p != phase_a_n) {
#ifdef CONFIG_EC_TYPE_1P1D
            (void)phase_b_p;

            if (!phase_a_n) {
                if (phase_b_n) {
                    fan_evt = EC_EVT_I;
                } else {
                    fan_evt = EC_EVT_D;
                }
            }
#else
            if (phase_a_n) {
                if (phase_b_p & !phase_b_n) {
                    fan_evt = EC_EVT_I;
                }
                if (!phase_b_p & phase_b_n) {
                    fan_evt = EC_EVT_D;
                }
                if ((phase_b_p == phase_b_n) & !phase_b_n) {
                    fan_evt = EC_EVT_I;
                }
                if ((phase_b_p == phase_b_n) & phase_b_n) {
                    fan_evt = EC_EVT_D;
                }
            } else {
                if (phase_b_p & !phase_b_n) {
                    fan_evt = EC_EVT_D;
                }
                if (!phase_b_p & phase_b_n) {
                    fan_evt = EC_EVT_I;
                }
                if ((phase_b_p == phase_b_n) & !phase_b_n) {
                    fan_evt = EC_EVT_D;
                }
                if ((phase_b_p == phase_b_n) & phase_b_n) {
                    fan_evt = EC_EVT_I;
                }
            }
#endif
        }

        switch (fan_evt) {
            case EC_EVT_N:
                if (button_p & !button_n) {
                    fan_evt = EC_EVT_N_B;
                }
                break;
            case EC_EVT_I:
                if (!button_n) {
                    fan_evt = EC_EVT_I_B;
                }
                break;
            case EC_EVT_D:
                if (!button_n) {
                    fan_evt = EC_EVT_D_B;
                }
                break;
            default:
                break;
        }

        if (fan_evt != EC_EVT_N) {
            xQueueSend(fan_evt_queue, &fan_evt, portMAX_DELAY);
        }

        vTaskDelayUntil(&xLastWakeTime, 1 / portTICK_RATE_MS);
    }
}

void ec_init(void)
{
    xTaskCreatePinnedToCore(ec_task, "ecT", 1920, NULL, 8, NULL, 1);
}
#endif

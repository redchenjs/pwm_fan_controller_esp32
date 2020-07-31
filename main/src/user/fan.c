/*
 * fan.c
 *
 *  Created on: 2020-04-19 12:43
 *      Author: Jack Chen <redchenjs@live.com>
 */

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/timer.h"

#include "core/os.h"
#include "core/app.h"

#include "user/ec.h"
#include "user/fan.h"

#define TAG "fan"

xQueueHandle fan_evt_queue = NULL;

static bool fan_mode = true;
static uint16_t fan_rpm = 0;
static uint16_t fan_duty = DEFAULT_FAN_DUTY;

static uint8_t env_cnt = 0;
static bool env_saved = true;

static double time_val = 0.0;
static double time_sum = 0.0;

static bool first_edge = true;
static bool period_done = true;

static void IRAM_ATTR tim_isr_handler(void *arg)
{
    timer_spinlock_take(TIMER_GROUP_0);

    timer_pause(TIMER_GROUP_0, TIMER_0);

    timer_group_clr_intr_status_in_isr(TIMER_GROUP_0, TIMER_0);
    timer_group_enable_alarm_in_isr(TIMER_GROUP_0, TIMER_0);

    uint32_t fan_evt = 0xff;
    xQueueSendFromISR(fan_evt_queue, &fan_evt, NULL);

    period_done = true;

    timer_spinlock_give(TIMER_GROUP_0);
}

static void IRAM_ATTR fan_isr_handler(void *arg)
{
    if (period_done) {
        if (first_edge) {
            if (!gpio_get_level(CONFIG_FAN_IN_PIN)) {
                first_edge = false;

                timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x00000000ULL);

                timer_start(TIMER_GROUP_0, TIMER_0);
            }
        } else {
            if (gpio_get_level(CONFIG_FAN_IN_PIN)) {
                first_edge = true;
                period_done = false;

                timer_get_counter_time_sec(TIMER_GROUP_0, TIMER_0, &time_val);
            }
        }
    }
}

static void tim_init(void)
{
    timer_config_t tim_conf = {
        .divider = 16,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .intr_type = TIMER_INTR_LEVEL,
    };
    timer_init(TIMER_GROUP_0, TIMER_0, &tim_conf);

    timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, 800000ULL);

    timer_enable_intr(TIMER_GROUP_0, TIMER_0);
    timer_isr_register(TIMER_GROUP_0, TIMER_0, tim_isr_handler, NULL, ESP_INTR_FLAG_IRAM, NULL);
}

static void pin_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = BIT64(CONFIG_FAN_IN_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = true,
        .pull_down_en = false,
        .intr_type = GPIO_PIN_INTR_ANYEDGE,
    };
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(CONFIG_FAN_IN_PIN, fan_isr_handler, NULL);
}

static void pwm_init(void)
{
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz         = 25000,
        .speed_mode      = LEDC_HIGH_SPEED_MODE,
        .timer_num       = LEDC_TIMER_1,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .channel    = LEDC_CHANNEL_1,
        .duty       = 0,
        .gpio_num   = CONFIG_FAN_OUT_PIN,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER_1,
    };
    ledc_channel_config(&ledc_channel);

    ledc_fade_func_install(0);
    ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, fan_duty, 0);
}

static void fan_task(void *pvParameter)
{
    uint8_t rpm_cnt = 0;
    uint32_t fan_evt = 0;

    tim_init();
    pin_init();
    pwm_init();

    ESP_LOGI(TAG, "started.");

    while (1) {
        xEventGroupWaitBits(
            user_event_group,
            FAN_RUN_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY
        );

        if (xQueueReceive(fan_evt_queue, &fan_evt, 500 / portTICK_RATE_MS)) {
            switch (fan_evt) {
#ifdef CONFIG_ENABLE_EC
                case EC_EVT_I: {
                    int16_t duty_tmp = fan_duty + 1;
                    fan_set_duty((duty_tmp < 255) ? duty_tmp : 255);
                    break;
                }
                case EC_EVT_D: {
                    int16_t duty_tmp = fan_duty - 1;
                    fan_set_duty((duty_tmp > 0) ? duty_tmp : 0);
                    break;
                }
                case EC_EVT_I_B: {
                    int16_t duty_tmp = fan_duty + 10;
                    fan_set_duty((duty_tmp < 255) ? duty_tmp : 255);
                    break;
                }
                case EC_EVT_D_B: {
                    int16_t duty_tmp = fan_duty - 10;
                    fan_set_duty((duty_tmp > 0) ? duty_tmp : 0);
                    break;
                }
#endif
                case 0xff:
                    if (rpm_cnt++ == 4) {
                        rpm_cnt = 0;

                        fan_rpm = 60.0 / time_sum;

                        time_sum = 0.0;
                    } else {
                        time_sum += time_val;
                    }
                    break;
                default:
                    break;
            }
        } else {
            rpm_cnt = 0;
            fan_rpm = 0;
            time_sum = 0.0;

            first_edge = true;
            period_done = true;
        }

        if (!env_saved && env_cnt++ == 50) {
            env_cnt = 0;

            env_saved = true;
            app_setenv("FAN_INIT_CFG", &fan_duty, sizeof(fan_duty));
        }
    }
}

void fan_set_duty(uint16_t val)
{
    if (fan_duty != val) {
        fan_duty = val;

        env_saved = false;
        ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, fan_duty, 0);
    }

    env_cnt = 0;
}

uint16_t fan_get_duty(void)
{
    return fan_duty;
}

uint16_t fan_get_rpm(void)
{
    return fan_rpm;
}

void fan_set_mode(bool val)
{
    fan_mode = val;

    if (fan_mode) {
        xEventGroupSetBits(user_event_group, FAN_RUN_BIT);

        gpio_intr_enable(CONFIG_FAN_IN_PIN);

        timer_start(TIMER_GROUP_0, TIMER_0);
        timer_enable_intr(TIMER_GROUP_0, TIMER_0);

        ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, fan_duty, 0);
    } else {
        xEventGroupClearBits(user_event_group, FAN_RUN_BIT);

        gpio_intr_disable(CONFIG_FAN_IN_PIN);

        timer_pause(TIMER_GROUP_0, TIMER_0);
        timer_disable_intr(TIMER_GROUP_0, TIMER_0);

        ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, 0, 0);
    }

    ESP_LOGI(TAG, "mode: %u", fan_mode);
}

bool fan_get_mode(void)
{
    return fan_mode;
}

bool fan_env_saved(void)
{
    return env_saved;
}

void fan_init(void)
{
    xEventGroupSetBits(user_event_group, FAN_RUN_BIT);

    size_t length = sizeof(fan_duty);
    app_getenv("FAN_INIT_CFG", &fan_duty, &length);

    fan_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    xTaskCreatePinnedToCore(fan_task, "fanT", 1920, NULL, 7, NULL, 1);
}

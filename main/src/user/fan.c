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
#include "user/pwr.h"

#define TAG "fan"

xQueueHandle fan_evt_queue = NULL;

static uint16_t fan_rpm = 0;

static uint8_t env_cnt = 0;
static bool env_saved = true;

static double time_val = 0.0;
static double time_sum = 0.0;

static bool first_edge = true;
static bool period_done = true;

static fan_mode_t fan_mode = FAN_MODE_IDX_ON;
static fan_conf_t fan_conf = {
    .duty    = DEFAULT_FAN_DUTY,
    .color_h = DEFAULT_FAN_COLOR_H,
    .color_s = DEFAULT_FAN_COLOR_S,
    .color_l = DEFAULT_FAN_COLOR_L
};

#ifdef CONFIG_ENABLE_FAN_RGB
static float hue2rgb(float v1, float v2, float vH)
{
    if (vH < 0.0) {
        vH += 1.0;
    } else if (vH > 1.0) {
        vH -= 1.0;
    }

    if (6.0 * vH < 1.0) {
        return v1 + (v2 - v1) * 6.0 * vH;
    } else if (2.0 * vH < 1.0) {
        return v2;
    } else if (3.0 * vH < 2.0) {
        return v1 + (v2 - v1) * (2.0 / 3.0 - vH) * 6.0;
    } else {
        return v1;
    }
}

static uint32_t hsl2rgb(float H, float S, float L)
{
    float v1, v2;
    uint8_t R, G, B;

    if (S == 0.0) {
        R = 255.0 * L;
        G = 255.0 * L;
        B = 255.0 * L;
    } else {
        if (L < 0.5) {
            v2 = L * (1.0 + S);
        } else {
            v2 = (L + S) - (L * S);
        }

        v1 = 2.0 * L - v2;

        R = 255.0 * hue2rgb(v1, v2, H + 1.0 / 3.0);
        G = 255.0 * hue2rgb(v1, v2, H);
        B = 255.0 * hue2rgb(v1, v2, H - 1.0 / 3.0);
    }

    return (uint32_t)(R << 16 | G << 8 | B);
}
#endif

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
        .intr_type = TIMER_INTR_LEVEL
    };
    timer_init(TIMER_GROUP_0, TIMER_0, &tim_conf);

    timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, 1000000ULL);

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
        .intr_type = GPIO_PIN_INTR_ANYEDGE
    };
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(CONFIG_FAN_IN_PIN, fan_isr_handler, NULL);
}

static void pwm_init(void)
{
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 25000,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_num = LEDC_TIMER_1,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .channel = LEDC_CHANNEL_1,
        .duty = 0,
        .gpio_num = CONFIG_FAN_OUT_PIN,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .hpoint = 0,
        .timer_sel = LEDC_TIMER_1
    };
    ledc_channel_config(&ledc_channel);

    ledc_fade_func_install(0);
    ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, fan_conf.duty, 0);

#ifdef CONFIG_ENABLE_FAN_RGB
    ledc_channel.channel = LEDC_CHANNEL_2;
    ledc_channel.gpio_num = CONFIG_FAN_RGB_R_PIN;
    ledc_channel_config(&ledc_channel);

    ledc_channel.channel = LEDC_CHANNEL_3;
    ledc_channel.gpio_num = CONFIG_FAN_RGB_G_PIN;
    ledc_channel_config(&ledc_channel);

    ledc_channel.channel = LEDC_CHANNEL_4;
    ledc_channel.gpio_num = CONFIG_FAN_RGB_B_PIN;
    ledc_channel_config(&ledc_channel);

    uint32_t pixel_color = hsl2rgb(fan_conf.color_h / 511.0, fan_conf.color_s / 255.0, fan_conf.color_l / 511.0);

    ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_2, 0xff & (pixel_color >> 16), 0);
    ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_3, 0xff & (pixel_color >> 8), 0);
    ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_4, 0xff & (pixel_color), 0);
#endif
}

static void fan_task(void *pvParameter)
{
    uint8_t rpm_cnt = 0;
    uint32_t fan_evt = 0;

    tim_init();
    pin_init();
    pwm_init();

    xEventGroupSetBits(user_event_group, FAN_CTRL_RUN_BIT);

    ESP_LOGI(TAG, "started.");

    while (1) {
        xEventGroupWaitBits(
            user_event_group,
            FAN_CTRL_RUN_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY
        );

        if (xQueueReceive(fan_evt_queue, &fan_evt, 500 / portTICK_RATE_MS)) {
            switch (fan_evt) {
#ifdef CONFIG_ENABLE_ENCODER
                case EC_EVT_I: {
                    int16_t duty_tmp = fan_conf.duty + 1;
                    fan_conf.duty = (duty_tmp < 255) ? duty_tmp : 255;
                    fan_set_conf(&fan_conf);
                    break;
                }
                case EC_EVT_D: {
                    int16_t duty_tmp = fan_conf.duty - 1;
                    fan_conf.duty = (duty_tmp > 0) ? duty_tmp : 0;
                    fan_set_conf(&fan_conf);
                    break;
                }
                case EC_EVT_I_B: {
                    int16_t duty_tmp = fan_conf.duty + 10;
                    fan_conf.duty = (duty_tmp < 255) ? duty_tmp : 255;
                    fan_set_conf(&fan_conf);
                    break;
                }
                case EC_EVT_D_B: {
                    int16_t duty_tmp = fan_conf.duty - 10;
                    fan_conf.duty = (duty_tmp > 0) ? duty_tmp : 0;
                    fan_set_conf(&fan_conf);
                    break;
                }
#endif
                case 0xff:
                    if (rpm_cnt++ == 3) {
                        rpm_cnt = 0;

                        fan_rpm = 45.0 / time_sum;

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

        fan_env_save();
        pwr_env_save();
    }
}

uint16_t fan_get_rpm(void)
{
    return fan_rpm;
}

void fan_set_mode(fan_mode_t idx)
{
    fan_mode = idx;

    if (fan_mode == FAN_MODE_IDX_ON) {
        xEventGroupSetBits(user_event_group, FAN_CTRL_RUN_BIT);

        gpio_intr_enable(CONFIG_FAN_IN_PIN);

        timer_start(TIMER_GROUP_0, TIMER_0);
        timer_enable_intr(TIMER_GROUP_0, TIMER_0);

        ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, fan_conf.duty, 0);
    } else {
        xEventGroupClearBits(user_event_group, FAN_CTRL_RUN_BIT);

        gpio_intr_disable(CONFIG_FAN_IN_PIN);

        timer_pause(TIMER_GROUP_0, TIMER_0);
        timer_disable_intr(TIMER_GROUP_0, TIMER_0);

        ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, 0, 0);
    }

    ESP_LOGI(TAG, "mode: %u", fan_mode);
}

fan_mode_t fan_get_mode(void)
{
    return fan_mode;
}

void fan_set_conf(fan_conf_t *cfg)
{
    fan_conf.duty = cfg->duty;

    ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, fan_conf.duty, 0);

#ifdef CONFIG_ENABLE_FAN_RGB
    fan_conf.color_h = cfg->color_h;
    fan_conf.color_s = cfg->color_s;
    fan_conf.color_l = cfg->color_l;

    uint32_t pixel_color = hsl2rgb(fan_conf.color_h / 511.0, fan_conf.color_s / 255.0, fan_conf.color_l / 511.0);

    ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_2, 0xff & (pixel_color >> 16), 0);
    ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_3, 0xff & (pixel_color >> 8), 0);
    ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_4, 0xff & (pixel_color), 0);

    ESP_LOGI(TAG, "duty: 0x%02X, hue: 0x%03X, saturation: 0x%02X, lightness: 0x%03X", fan_conf.duty, fan_conf.color_h, fan_conf.color_s, fan_conf.color_l);
#else
    ESP_LOGI(TAG, "duty: 0x%02X", fan_conf.duty);
#endif

    env_saved = false;
    env_cnt = 0;
}

fan_conf_t *fan_get_conf(void)
{
    return &fan_conf;
}

void fan_env_save(void)
{
    if (!env_saved && env_cnt++ == 50) {
        env_cnt = 0;

        env_saved = true;
        app_setenv("FAN_INIT_CFG", &fan_conf, sizeof(fan_conf));
    }
}

bool fan_env_saved(void)
{
    return env_saved;
}

void fan_init(void)
{
    size_t length = sizeof(fan_conf);
    app_getenv("FAN_INIT_CFG", &fan_conf, &length);

    fan_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    xTaskCreatePinnedToCore(fan_task, "fanT", 1920, NULL, 7, NULL, 1);
}

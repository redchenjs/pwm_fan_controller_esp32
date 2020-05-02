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

#include "driver/adc.h"
#include "driver/dac.h"
#include "driver/ledc.h"
#include "driver/timer.h"

#include "user/fan.h"

#define TAG "fan"

enum qc_idx {
    QC_IDX_5V  = 0x0,
    QC_IDX_9V  = 0x1,
    QC_IDX_12V = 0x2,

    QC_IDX_MAX
};

static uint8_t duty_set = 0;
static uint8_t fan_duty = 0;
static uint16_t fan_rpm = 0;
static double time_val = 0.0;
static bool first_edge = true;
static bool period_done = true;
static xQueueHandle gpio_evt_queue = NULL;

#ifdef CONFIG_ENABLE_QC
static void qc_init(uint8_t qc_vol)
{
    int dp_raw = 0, dp_cnt = 0;

    adc2_config_channel_atten(ADC_CHANNEL_9, ADC_ATTEN_DB_0);

    dac_output_enable(DAC_CHANNEL_1);
    dac_output_voltage(DAC_CHANNEL_1, 255 * (0.325 / 3.3));

    adc2_get_raw(ADC_CHANNEL_9, ADC_WIDTH_BIT_12, &dp_raw);

    if (dp_raw < 100) {
        ESP_LOGI(TAG, "SDP 5V");
        dac_output_disable(DAC_CHANNEL_1);
        return;
    }

    vTaskDelay(1250 / portTICK_RATE_MS);

    do {
        adc2_get_raw(ADC_CHANNEL_9, ADC_WIDTH_BIT_12, &dp_raw);

        vTaskDelay(10 / portTICK_RATE_MS);
    } while (dp_raw > 100 && ++dp_cnt < 100);

    if (dp_raw > 100) {
        ESP_LOGI(TAG, "DCP 5V");
        return;
    }

    switch (qc_vol) {
        case QC_IDX_9V:
            ESP_LOGI(TAG, "QC 2.0 9V");
            dac_output_enable(DAC_CHANNEL_2);
            dac_output_voltage(DAC_CHANNEL_1, 255 * (3.3 / 3.3));
            dac_output_voltage(DAC_CHANNEL_2, 255 * (0.6 / 3.3));
            break;
        case QC_IDX_12V:
            ESP_LOGI(TAG, "QC 2.0 12V");
            dac_output_enable(DAC_CHANNEL_2);
            dac_output_voltage(DAC_CHANNEL_1, 255 * (0.6 / 3.3));
            dac_output_voltage(DAC_CHANNEL_2, 255 * (0.6 / 3.3));
            break;
        default:
            ESP_LOGI(TAG, "DCP 5V");
            break;
    }
}
#endif

static void IRAM_ATTR tim_isr_handler(void *arg)
{
    timer_spinlock_take(TIMER_GROUP_0);

    timer_pause(TIMER_GROUP_0, TIMER_0);

    timer_group_clr_intr_status_in_isr(TIMER_GROUP_0, TIMER_0);
    timer_group_enable_alarm_in_isr(TIMER_GROUP_0, TIMER_0);

    uint32_t gpio_evt = 1;
    xQueueSendFromISR(gpio_evt_queue, &gpio_evt, NULL);

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

#ifdef CONFIG_ENABLE_INPUT
static void IRAM_ATTR key_isr_handler(void *arg)
{
    int16_t duty = duty_set;

    if (gpio_get_level(CONFIG_BUTTON_PIN)) {
        if (gpio_get_level(CONFIG_PHASE_A_PIN)) {
            if (gpio_get_level(CONFIG_PHASE_B_PIN)) {
                duty++;
            } else {
                duty--;
            }
        } else {
            if (!gpio_get_level(CONFIG_PHASE_B_PIN)) {
                duty++;
            } else {
                duty--;
            }
        }
    } else {
        if (gpio_get_level(CONFIG_PHASE_A_PIN)) {
            if (gpio_get_level(CONFIG_PHASE_B_PIN)) {
                duty += 10;
            } else {
                duty -= 10;
            }
        } else {
            if (!gpio_get_level(CONFIG_PHASE_B_PIN)) {
                duty += 10;
            } else {
                duty -= 10;
            }
        }
    }

    if (duty < 0)   duty = 0;
    if (duty > 255) duty = 255;

    duty_set = duty;

    uint32_t gpio_evt = 0;
    xQueueSendFromISR(gpio_evt_queue, &gpio_evt, NULL);
}
#endif

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

    timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, 500000ULL);

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

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    gpio_install_isr_service(0);
    gpio_isr_handler_add(CONFIG_FAN_IN_PIN, fan_isr_handler, NULL);

#ifdef CONFIG_ENABLE_INPUT
    io_conf.pin_bit_mask = BIT64(CONFIG_PHASE_A_PIN);
    gpio_config(&io_conf);

    gpio_isr_handler_add(CONFIG_PHASE_A_PIN, key_isr_handler, NULL);

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = BIT64(CONFIG_PHASE_B_PIN);
    gpio_config(&io_conf);

    io_conf.pin_bit_mask = BIT64(CONFIG_BUTTON_PIN);
    gpio_config(&io_conf);
#endif
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
}

static void fan_task(void *pvParameter)
{
    uint8_t rpm_cnt = 0;
    double rpm_sum = 0.0;
    uint32_t gpio_evt = 0;

#ifdef CONFIG_ENABLE_QC
    qc_init(QC_IDX_12V);
#endif

    tim_init();
    pin_init();
    pwm_init();

    ESP_LOGI(TAG, "started.");

    while (1) {
        if (xQueueReceive(gpio_evt_queue, &gpio_evt, 500 / portTICK_RATE_MS)) {
            switch (gpio_evt) {
#ifdef CONFIG_ENABLE_INPUT
                case 0:
                    fan_set_duty(duty_set);
                    break;
#endif
                case 1:
                    if (rpm_cnt++ == 5) {
                        rpm_cnt = 0;

                        fan_rpm = rpm_sum / 5.0;

                        rpm_sum = 0.0;
                    } else {
                        rpm_sum += 15.0 / time_val;
                    }
                    break;
                default:
                    break;
            }
        } else {
            first_edge = true;
            period_done = true;

            fan_rpm = 0;
            rpm_cnt = 0;
            rpm_sum = 0.0;
        }
    }
}

void fan_set_duty(uint8_t val)
{
    if (fan_duty != val) {
        fan_duty = val;
        ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, fan_duty, 0);
    }
}

uint8_t fan_get_duty(void)
{
    return fan_duty;
}

uint16_t fan_get_rpm(void)
{
    return fan_rpm;
}

void fan_init(void)
{
    xTaskCreatePinnedToCore(fan_task, "fanT", 1920, NULL, 8, NULL, 1);
}

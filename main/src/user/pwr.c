/*
 * pwr.c
 *
 *  Created on: 2020-05-25 13:39
 *      Author: Jack Chen <redchenjs@live.com>
 */

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/adc.h"
#include "driver/dac.h"

#include "core/app.h"
#include "user/pwr.h"

#define TAG "pwr"

static bool qc_mode = false;
static pwr_mode_t pwr_mode = PWR_MODE_IDX_DC;
static pwr_mode_t env_mode = PWR_MODE_IDX_DC;

static uint8_t env_cnt = 0;
static bool env_saved = true;

static char pwr_mode_str[][8] = {
    "DC IN",
    "SDP 5V",
    "DCP 5V",
    "QC 5V",
    "QC 9V",
    "QC 12V",
};

void pwr_set_mode(pwr_mode_t idx)
{
    if (!qc_mode) {
        return;
    }

    switch (idx) {
        default:
        case PWR_MODE_IDX_QC_5V:
            pwr_mode = PWR_MODE_IDX_QC_5V;

            dac_output_voltage(DAC_CHANNEL_1, 255 * (0.6 / 3.3));
            dac_output_voltage(DAC_CHANNEL_2, 255 * (0.0 / 3.3));
            dac_output_enable(DAC_CHANNEL_1);
            dac_output_enable(DAC_CHANNEL_2);
            break;
        case PWR_MODE_IDX_QC_9V:
            pwr_mode = PWR_MODE_IDX_QC_9V;

            dac_output_voltage(DAC_CHANNEL_1, 255 * (3.3 / 3.3));
            dac_output_voltage(DAC_CHANNEL_2, 255 * (0.6 / 3.3));
            dac_output_enable(DAC_CHANNEL_1);
            dac_output_enable(DAC_CHANNEL_2);
            break;
        case PWR_MODE_IDX_QC_12V:
            pwr_mode = PWR_MODE_IDX_QC_12V;

            dac_output_voltage(DAC_CHANNEL_1, 255 * (0.6 / 3.3));
            dac_output_voltage(DAC_CHANNEL_2, 255 * (0.6 / 3.3));
            dac_output_enable(DAC_CHANNEL_1);
            dac_output_enable(DAC_CHANNEL_2);
            break;
    }

    if (env_mode != pwr_mode) {
        env_mode = pwr_mode;

        env_saved = false;
        ESP_LOGI(TAG, "%s", pwr_get_mode_str());
    }

    env_cnt = 0;
}

pwr_mode_t pwr_get_mode(void)
{
    return pwr_mode;
}

char *pwr_get_mode_str(void)
{
    return pwr_mode_str[pwr_mode];
}

void pwr_env_save(void)
{
    if (!env_saved && env_cnt++ == 50) {
        env_cnt = 0;

        env_saved = true;
        app_setenv("PWR_INIT_CFG", &env_mode, sizeof(env_mode));
    }
}

bool pwr_env_saved(void)
{
    return env_saved;
}

void pwr_deinit(void)
{
    dac_output_disable(DAC_CHANNEL_1);
    dac_output_disable(DAC_CHANNEL_2);
}

void pwr_init(void)
{
    int dp_raw = 0, dp_cnt = 0;

    adc2_config_channel_atten(ADC_CHANNEL_9, ADC_ATTEN_DB_0);

    dac_output_disable(DAC_CHANNEL_1);
    dac_output_disable(DAC_CHANNEL_2);

    vTaskDelay(50 / portTICK_RATE_MS);

    dac_output_voltage(DAC_CHANNEL_1, 255 * (0.325 / 3.3));
    dac_output_enable(DAC_CHANNEL_1);

    adc2_get_raw(ADC_CHANNEL_9, ADC_WIDTH_BIT_12, &dp_raw);

    if (dp_raw < 255) {
        pwr_mode = PWR_MODE_IDX_SDP;

        dac_output_disable(DAC_CHANNEL_1);

        goto pwr_exit;
    }

    do {
        adc2_get_raw(ADC_CHANNEL_9, ADC_WIDTH_BIT_12, &dp_raw);

        vTaskDelay(10 / portTICK_RATE_MS);
    } while (dp_raw > 255 && ++dp_cnt < 150);

    dac_output_disable(DAC_CHANNEL_1);

    if (dp_raw > 255) {
        pwr_mode = PWR_MODE_IDX_DCP;

        goto pwr_exit;
    }

    qc_mode = true;

    size_t length = sizeof(env_mode);
    app_getenv("PWR_INIT_CFG", &env_mode, &length);

    pwr_set_mode(env_mode);

pwr_exit:
    ESP_LOGI(TAG, "started.");
}

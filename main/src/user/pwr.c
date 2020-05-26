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

#include "user/pwr.h"

#define TAG "pwr"

static pwr_idx_t pwr_mode = PWR_IDX_DC;

static char pwr_mode_str[][8] = {
    "DC IN",
    "SDP 5V",
    "DCP 5V",
    "QC 5V",
    "QC 9V",
    "QC 12V",
};

pwr_idx_t pwr_get_mode(void)
{
    return pwr_mode;
}

char *pwr_get_mode_str(void)
{
    return pwr_mode_str[pwr_mode];
}

void pwr_init(pwr_idx_t idx)
{
    int dp_raw = 0, dp_cnt = 0;

    dac_output_voltage(DAC_CHANNEL_1, 0);
    dac_output_voltage(DAC_CHANNEL_2, 0);
    dac_output_disable(DAC_CHANNEL_1);
    dac_output_disable(DAC_CHANNEL_2);

    adc2_config_channel_atten(ADC_CHANNEL_9, ADC_ATTEN_DB_0);

    dac_output_enable(DAC_CHANNEL_1);
    dac_output_voltage(DAC_CHANNEL_1, 255 * (0.325 / 3.3));

    vTaskDelay(50 / portTICK_RATE_MS);

    adc2_get_raw(ADC_CHANNEL_9, ADC_WIDTH_BIT_12, &dp_raw);

    if (dp_raw < 255) {
        pwr_mode = PWR_IDX_SDP;

        dac_output_disable(DAC_CHANNEL_1);

        goto pwr_exit;
    }

    vTaskDelay(1000 / portTICK_RATE_MS);

    do {
        adc2_get_raw(ADC_CHANNEL_9, ADC_WIDTH_BIT_12, &dp_raw);

        vTaskDelay(10 / portTICK_RATE_MS);
    } while (dp_raw > 255 && ++dp_cnt < 100);

    if (dp_raw > 255) {
        pwr_mode = PWR_IDX_DCP;

        goto pwr_exit;
    }

    switch (idx) {
        case PWR_IDX_QC_5V:
            pwr_mode = PWR_IDX_QC_5V;

            dac_output_enable(DAC_CHANNEL_2);
            dac_output_voltage(DAC_CHANNEL_1, 255 * (0.6 / 3.3));
            dac_output_voltage(DAC_CHANNEL_2, 255 * (0.0 / 3.3));
            break;
        case PWR_IDX_QC_9V:
            pwr_mode = PWR_IDX_QC_9V;

            dac_output_enable(DAC_CHANNEL_2);
            dac_output_voltage(DAC_CHANNEL_1, 255 * (3.3 / 3.3));
            dac_output_voltage(DAC_CHANNEL_2, 255 * (0.6 / 3.3));
            break;
        case PWR_IDX_QC_12V:
            pwr_mode = PWR_IDX_QC_12V;

            dac_output_enable(DAC_CHANNEL_2);
            dac_output_voltage(DAC_CHANNEL_1, 255 * (0.6 / 3.3));
            dac_output_voltage(DAC_CHANNEL_2, 255 * (0.6 / 3.3));
            break;
        default:
            pwr_mode = PWR_IDX_DCP;

            break;
    }

pwr_exit:
    ESP_LOGI(TAG, "%s", pwr_get_mode_str());
}

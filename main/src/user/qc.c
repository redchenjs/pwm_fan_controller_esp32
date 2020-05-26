/*
 * qc.c
 *
 *  Created on: 2020-05-25 13:39
 *      Author: Jack Chen <redchenjs@live.com>
 */

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/adc.h"
#include "driver/dac.h"

#include "user/qc.h"

#define TAG "qc"

static qc_idx_t qc_mode = QC_IDX_DC;

static char qc_mode_str[][8] = {
    "DC",
    "SDP 5V",
    "DCP 5V",
    "QC2 5V",
    "QC2 9V",
    "QC2 12V",
};

qc_idx_t qc_get_mode(void)
{
    return qc_mode;
}

char *qc_get_mode_str(void)
{
    return qc_mode_str[qc_mode];
}

void qc_init(qc_idx_t idx)
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
        qc_mode = QC_IDX_SDP;

        dac_output_disable(DAC_CHANNEL_1);

        goto qc_exit;
    }

    vTaskDelay(1000 / portTICK_RATE_MS);

    do {
        adc2_get_raw(ADC_CHANNEL_9, ADC_WIDTH_BIT_12, &dp_raw);

        vTaskDelay(10 / portTICK_RATE_MS);
    } while (dp_raw > 255 && ++dp_cnt < 100);

    if (dp_raw > 255) {
        qc_mode = QC_IDX_DCP;

        goto qc_exit;
    }

    switch (idx) {
        case QC_IDX_5V:
            qc_mode = QC_IDX_5V;

            dac_output_enable(DAC_CHANNEL_2);
            dac_output_voltage(DAC_CHANNEL_1, 255 * (0.6 / 3.3));
            dac_output_voltage(DAC_CHANNEL_2, 255 * (0.0 / 3.3));
            break;
        case QC_IDX_9V:
            qc_mode = QC_IDX_9V;

            dac_output_enable(DAC_CHANNEL_2);
            dac_output_voltage(DAC_CHANNEL_1, 255 * (3.3 / 3.3));
            dac_output_voltage(DAC_CHANNEL_2, 255 * (0.6 / 3.3));
            break;
        case QC_IDX_12V:
            qc_mode = QC_IDX_12V;

            dac_output_enable(DAC_CHANNEL_2);
            dac_output_voltage(DAC_CHANNEL_1, 255 * (0.6 / 3.3));
            dac_output_voltage(DAC_CHANNEL_2, 255 * (0.6 / 3.3));
            break;
        default:
            qc_mode = QC_IDX_DCP;

            break;
    }

qc_exit:
    ESP_LOGI(TAG, "%s", qc_get_mode_str());
}

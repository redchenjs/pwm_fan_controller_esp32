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

#ifdef CONFIG_ENABLE_QC
void qc_init(qc_idx_t idx)
{
    int dp_raw = 0, dp_cnt = 0;

    dac_output_voltage(DAC_CHANNEL_1, 0);
    dac_output_voltage(DAC_CHANNEL_2, 0);
    dac_output_disable(DAC_CHANNEL_1);
    dac_output_disable(DAC_CHANNEL_2);

    vTaskDelay(50 / portTICK_RATE_MS);

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

    switch (idx) {
        case QC_IDX_5V:
            ESP_LOGI(TAG, "QC 2.0 5V");
            dac_output_enable(DAC_CHANNEL_2);
            dac_output_voltage(DAC_CHANNEL_1, 255 * (0.6 / 3.3));
            dac_output_voltage(DAC_CHANNEL_2, 255 * (0.0 / 3.3));
            break;
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

/*
 * gui.c
 *
 *  Created on: 2020-04-19 11:12
 *      Author: Jack Chen <redchenjs@live.com>
 */

#include <stdio.h>

#include "esp_log.h"
#include "driver/gpio.h"

#include "gfx.h"

#include "core/os.h"
#include "board/ina219.h"

#include "user/pwr.h"
#include "user/fan.h"

#define TAG "gui"

GDisplay *gui_gdisp = NULL;

static bool gui_mode = true;
static char text_buff[32] = {0};

static void gui_task(void *pvParameter)
{
    font_t gui_font;
    portTickType xLastWakeTime;

    gfxInit();

    gui_gdisp = gdispGetDisplay(0);
    gui_font = gdispOpenFont("DejaVuSans32");

    ESP_LOGI(TAG, "started.");

    snprintf(text_buff, sizeof(text_buff), "PWM:");
    gdispGFillStringBox(gui_gdisp, 2, 2, 93, 32, text_buff, gui_font, Yellow, Black, justifyLeft);

    snprintf(text_buff, sizeof(text_buff), "RPM:");
    gdispGFillStringBox(gui_gdisp, 2, 34, 93, 32, text_buff, gui_font, Cyan, Black, justifyLeft);

    snprintf(text_buff, sizeof(text_buff), "PWR:");
    gdispGFillStringBox(gui_gdisp, 2, 67, 93, 32, text_buff, gui_font, Magenta, Black, justifyLeft);

    while (1) {
        if (gui_mode) {
            xLastWakeTime = xTaskGetTickCount();

            gdispGSetBacklight(gui_gdisp, 255);

            snprintf(text_buff, sizeof(text_buff), "%u", fan_get_duty());
            gdispGFillStringBox(gui_gdisp, 95, 2, 143, 32, text_buff, gui_font, Yellow, Black, justifyRight);

            snprintf(text_buff, sizeof(text_buff), "%u", fan_get_rpm());
            gdispGFillStringBox(gui_gdisp, 95, 34, 143, 32, text_buff, gui_font, Cyan, Black, justifyRight);

            snprintf(text_buff, sizeof(text_buff), "%s", pwr_get_mode_str());
            gdispGFillStringBox(gui_gdisp, 95, 67, 143, 32, text_buff, gui_font, Magenta, Black, justifyRight);

            snprintf(text_buff, sizeof(text_buff), "%5.2fV", ina219_get_bus_voltage_v());
            gdispGFillStringBox(gui_gdisp, 2, 100, 118, 32, text_buff, gui_font, Lime, Black, justifyRight);

            snprintf(text_buff, sizeof(text_buff), "%5.3fA", ina219_get_current_ma() / 1000.0);
            gdispGFillStringBox(gui_gdisp, 120, 100, 118, 32, text_buff, gui_font, Orange, Black, justifyRight);

            gdispGFlush(gui_gdisp);

            vTaskDelayUntil(&xLastWakeTime, 25 / portTICK_RATE_MS);
        } else {
            if (xEventGroupGetBits(user_event_group) & GUI_RELOAD_BIT) {
                xEventGroupClearBits(user_event_group, GUI_RELOAD_BIT);
            }

            gdispGSetBacklight(gui_gdisp, 0);

            xEventGroupWaitBits(
                user_event_group,
                GUI_RELOAD_BIT,
                pdTRUE,
                pdFALSE,
                portMAX_DELAY
            );
        }
    }
}

void gui_set_mode(bool val)
{
    gui_mode = val;

    xEventGroupSetBits(user_event_group, GUI_RELOAD_BIT);

    ESP_LOGI(TAG, "mode: %u", gui_mode);
}

bool gui_get_mode(void)
{
    return gui_mode;
}

void gui_init(void)
{
    xTaskCreatePinnedToCore(gui_task, "guiT", 1920, NULL, 7, NULL, 0);
}

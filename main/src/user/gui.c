/*
 * gui.c
 *
 *  Created on: 2020-04-19 11:12
 *      Author: Jack Chen <redchenjs@live.com>
 */

#include <math.h>
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

static GTimer gui_flush_timer;

static bool gui_mode = true;
static char text_buff[32] = {0};

static void gui_flush_task(void *pvParameter)
{
    gdispGFlush(gui_gdisp);
}

static void gui_task(void *pvParameter)
{
    font_t gui_font;
    portTickType xLastWakeTime;

    gfxInit();

    gui_gdisp = gdispGetDisplay(0);
    gui_font = gdispOpenFont("DejaVuSans32");

    gtimerStart(&gui_flush_timer, gui_flush_task, NULL, TRUE, TIME_INFINITE);

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

            snprintf(text_buff, sizeof(text_buff), "%u%s", fan_get_duty(), fan_env_saved() ? "" : "*");
            gdispGFillStringBox(gui_gdisp, 95, 2, 143, 32, text_buff, gui_font, Yellow, Black, justifyRight);

            snprintf(text_buff, sizeof(text_buff), "%u", fan_get_rpm());
            gdispGFillStringBox(gui_gdisp, 95, 34, 143, 32, text_buff, gui_font, Cyan, Black, justifyRight);

            snprintf(text_buff, sizeof(text_buff), "%s%s", pwr_get_mode_str(), pwr_env_saved() ? "" : "*");
            gdispGFillStringBox(gui_gdisp, 95, 67, 143, 32, text_buff, gui_font, Magenta, Black, justifyRight);

            float voltage = ina219_get_bus_voltage_mv() * 0.001;
            if (voltage < 10.00) {
                snprintf(text_buff, sizeof(text_buff), "%4.3fV", fabs(voltage));
            } else {
                snprintf(text_buff, sizeof(text_buff), "%4.2fV", fabs(voltage));
            }
            gdispGFillStringBox(gui_gdisp, 2, 100, 118, 32, text_buff, gui_font, Lime, Black, justifyRight);

            float current = ina219_get_current_ma() * 0.001;
            snprintf(text_buff, sizeof(text_buff), "%4.3fA", fabs(current));
            if (current < 0.000) {
                gdispGFillStringBox(gui_gdisp, 120, 100, 118, 32, text_buff, gui_font, SkyBlue, Black, justifyRight);
            } else {
                gdispGFillStringBox(gui_gdisp, 120, 100, 118, 32, text_buff, gui_font, Orange, Black, justifyRight);
            }

            gtimerJab(&gui_flush_timer);

            vTaskDelayUntil(&xLastWakeTime, 20 / portTICK_RATE_MS);
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
    xTaskCreatePinnedToCore(gui_task, "guiT", 1920, NULL, 7, NULL, 1);
}

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
#include "user/qc.h"
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
    gdispGFillStringBox(gui_gdisp, 5, 4, 90, 42, text_buff, gui_font, Yellow, Black, justifyLeft);

    snprintf(text_buff, sizeof(text_buff), "RPM:");
    gdispGFillStringBox(gui_gdisp, 5, 46, 90, 42, text_buff, gui_font, Aqua, Black, justifyLeft);

    snprintf(text_buff, sizeof(text_buff), "PWR:");
    gdispGFillStringBox(gui_gdisp, 5, 88, 90, 42, text_buff, gui_font, Fuchsia, Black, justifyLeft);

    while (1) {
        if (gui_mode) {
            if (xEventGroupGetBits(user_event_group) & GUI_RELOAD_BIT) {
                xEventGroupClearBits(user_event_group, GUI_RELOAD_BIT);
                continue;
            }

            xLastWakeTime = xTaskGetTickCount();

            gdispGSetBacklight(gui_gdisp, 255);

            snprintf(text_buff, sizeof(text_buff), "%u", fan_get_duty());
            gdispGFillStringBox(gui_gdisp, 95, 4, 140, 42, text_buff, gui_font, Yellow, Black, justifyRight);

            snprintf(text_buff, sizeof(text_buff), "%u", fan_get_rpm());
            gdispGFillStringBox(gui_gdisp, 95, 46, 140, 42, text_buff, gui_font, Aqua, Black, justifyRight);

            snprintf(text_buff, sizeof(text_buff), "%s", qc_get_mode_str());
            gdispGFillStringBox(gui_gdisp, 95, 88, 140, 42, text_buff, gui_font, Fuchsia, Black, justifyRight);

            gdispGFlush(gui_gdisp);

            vTaskDelayUntil(&xLastWakeTime, 16 / portTICK_RATE_MS);
        } else {
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

    if (gui_mode) {
        xEventGroupSetBits(user_event_group, GUI_RELOAD_BIT);
    } else {
        xEventGroupClearBits(user_event_group, GUI_RELOAD_BIT);
    }

    ESP_LOGI(TAG, "mode: %u", gui_mode);
}

bool gui_get_mode(void)
{
    return gui_mode;
}

void gui_init(void)
{
    xEventGroupSetBits(user_event_group, GUI_RELOAD_BIT);

    xTaskCreatePinnedToCore(gui_task, "guiT", 1920, NULL, 7, NULL, 0);
}

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

#include "user/fan.h"

#define TAG "gui"

GDisplay *gui_gdisp = NULL;

static char text_buff[32] = {0};

static void gui_task(void *pvParameter)
{
    font_t gui_font;
    portTickType xLastWakeTime;

    gfxInit();

    gui_gdisp = gdispGetDisplay(0);
    gui_font = gdispOpenFont("DejaVuSans32");

    ESP_LOGI(TAG, "started.");

    snprintf(text_buff, sizeof(text_buff), "SET:");
    gdispGFillStringBox(gui_gdisp, 25, 25, 80, 40, text_buff, gui_font, Red, Black, justifyLeft);

    snprintf(text_buff, sizeof(text_buff), "RPM:");
    gdispGFillStringBox(gui_gdisp, 25, 75, 80, 40, text_buff, gui_font, Lime, Black, justifyLeft);

    gdispGSetBacklight(gui_gdisp, 255);

    while (1) {
        xLastWakeTime = xTaskGetTickCount();

        snprintf(text_buff, sizeof(text_buff), "%u", fan_get_duty());
        gdispGFillStringBox(gui_gdisp, 110, 25, 105, 40, text_buff, gui_font, Red, Black, justifyRight);

        snprintf(text_buff, sizeof(text_buff), "%u", fan_get_rpm());
        gdispGFillStringBox(gui_gdisp, 110, 75, 105, 40, text_buff, gui_font, Lime, Black, justifyRight);

        gdispGFlush(gui_gdisp);

        vTaskDelayUntil(&xLastWakeTime, 50 / portTICK_RATE_MS);
    }
}

void gui_init(void)
{
    xTaskCreatePinnedToCore(gui_task, "guiT", 1920, NULL, 7, NULL, 1);
}

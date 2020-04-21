/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 */

#include "gdriver.c"

#undef GDISP_DRIVER_LIST

#define GDISP_DRIVER_LIST GDISPVMT_ST7789
#include "drivers/gdisp/ST7789/gdisp_lld_ST7789.c"

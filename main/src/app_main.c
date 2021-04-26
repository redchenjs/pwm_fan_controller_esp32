/*
 * app_main.c
 *
 *  Created on: 2018-03-11 15:57
 *      Author: Jack Chen <redchenjs@live.com>
 */

#include "core/os.h"
#include "core/app.h"

#include "chip/bt.h"
#include "chip/nvs.h"
#include "chip/spi.h"
#include "chip/i2c.h"

#include "board/ina219.h"

#include "user/ec.h"
#include "user/key.h"
#include "user/pwr.h"
#include "user/fan.h"
#include "user/gui.h"
#include "user/led.h"
#include "user/ble_app.h"

static void core_init(void)
{
    app_print_info();

    os_init();
}

static void chip_init(void)
{
    nvs_init();

    bt_init();

#ifdef CONFIG_ENABLE_GUI
    spi_host_init();
#endif

#ifdef CONFIG_ENABLE_POWER_MONITOR
    i2c_host_init();
#endif
}

static void board_init(void)
{
#ifdef CONFIG_ENABLE_POWER_MONITOR
    ina219_init();
#endif
}

static void user_init(void)
{
#ifdef CONFIG_ENABLE_ENCODER
    ec_init();
#endif

#if defined(CONFIG_ENABLE_POWER_MODE_KEY) || defined(CONFIG_ENABLE_SLEEP_KEY)
    key_init();
#endif

#ifdef CONFIG_ENABLE_QC
    pwr_init();
#endif

    fan_init();

#ifdef CONFIG_ENABLE_GUI
    gui_init();
#endif

#ifdef CONFIG_ENABLE_LED
    led_init();
#endif

#ifdef CONFIG_ENABLE_BLE_CONTROL_IF
    ble_app_init();
#endif
}

int app_main(void)
{
    core_init();

    chip_init();

    board_init();

    user_init();

    return 0;
}

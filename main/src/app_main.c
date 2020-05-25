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

#include "user/qc.h"
#include "user/fan.h"
#include "user/gui.h"
#include "user/key.h"
#include "user/bt_app.h"
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
    hspi_init();
#endif

#ifdef CONFIG_ENABLE_POWER_MONITOR
    i2c0_init();
#endif
}

static void board_init(void) {}

static void user_init(void)
{
#ifdef CONFIG_ENABLE_QC
    qc_init(QC_IDX_12V);
#endif

    fan_init();

#ifdef CONFIG_ENABLE_GUI
    gui_init();
#endif

#ifdef CONFIG_ENABLE_ENCODER
    key_init();
#endif

#ifdef CONFIG_ENABLE_BLE_CONTROL_IF
    ble_app_init();
#endif

#ifdef CONFIG_ENABLE_OTA_OVER_SPP
    bt_app_init();
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

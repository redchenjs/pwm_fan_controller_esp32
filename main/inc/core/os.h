/*
 * os.h
 *
 *  Created on: 2018-03-04 20:07
 *      Author: Jack Chen <redchenjs@live.com>
 */

#ifndef INC_CORE_OS_H_
#define INC_CORE_OS_H_

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

typedef enum user_event_group_bits {
    OS_PWR_SLEEP_BIT   = BIT0,
    OS_PWR_RESET_BIT   = BIT1,

    BLE_GATTS_IDLE_BIT = BIT2,
    BLE_GATTS_LOCK_BIT = BIT3,

    FAN_RUN_BIT        = BIT4,
    KEY_RUN_BIT        = BIT5,
    GUI_RELOAD_BIT     = BIT6,
} user_event_group_bits_t;

extern EventGroupHandle_t user_event_group;

extern void os_pwr_sleep_wait(EventBits_t bits);
extern void os_pwr_reset_wait(EventBits_t bits);

extern void os_init(void);

#endif /* INC_CORE_OS_H_ */

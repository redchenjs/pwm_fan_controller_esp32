/*
 * fan.h
 *
 *  Created on: 2020-04-19 12:43
 *      Author: Jack Chen <redchenjs@live.com>
 */

#ifndef INC_USER_FAN_H_
#define INC_USER_FAN_H_

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define DEFAULT_FAN_DUTY 0x00

extern xQueueHandle fan_evt_queue;

extern void fan_set_duty(uint16_t val);
extern uint16_t fan_get_duty(void);
extern uint16_t fan_get_rpm(void);

extern void fan_set_mode(bool val);
extern bool fan_get_mode(void);

extern void fan_env_save(void);
extern bool fan_env_saved(void);

extern void fan_init(void);

#endif /* INC_USER_FAN_H_ */

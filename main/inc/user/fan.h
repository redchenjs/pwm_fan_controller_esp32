/*
 * fan.h
 *
 *  Created on: 2020-04-19 12:43
 *      Author: Jack Chen <redchenjs@live.com>
 */

#ifndef INC_USER_FAN_H_
#define INC_USER_FAN_H_

#include <stdint.h>

extern void fan_set_duty(uint8_t val);
extern uint8_t fan_get_duty(void);
extern uint16_t fan_get_rpm(void);

extern void fan_init(void);

#endif /* INC_USER_FAN_H_ */

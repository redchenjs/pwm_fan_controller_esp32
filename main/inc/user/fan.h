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

typedef enum {
    FAN_MODE_IDX_OFF = 0x00,
    FAN_MODE_IDX_ON  = 0x01
} fan_mode_t;

typedef struct {
    uint16_t duty;
    uint16_t color_h;
    uint16_t color_s;
    uint16_t color_l;
} fan_conf_t;

#define DEFAULT_FAN_DUTY    0x00
#define DEFAULT_FAN_COLOR_H 0xFF
#define DEFAULT_FAN_COLOR_S 0xFF
#define DEFAULT_FAN_COLOR_L 0xFF

extern xQueueHandle fan_evt_queue;

extern uint16_t fan_get_rpm(void);

extern void fan_set_mode(fan_mode_t idx);
extern fan_mode_t fan_get_mode(void);

extern void fan_set_conf(fan_conf_t *cfg);
extern fan_conf_t *fan_get_conf(void);

extern void fan_env_save(void);
extern bool fan_env_saved(void);

extern void fan_init(void);

#endif /* INC_USER_FAN_H_ */

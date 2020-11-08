/*
 * gui.h
 *
 *  Created on: 2020-04-19 11:12
 *      Author: Jack Chen <redchenjs@live.com>
 */

#ifndef INC_USER_GUI_H_
#define INC_USER_GUI_H_

typedef enum {
    GUI_MODE_IDX_ON  = 0x00,
    GUI_MODE_IDX_OFF = 0xFF
} gui_mode_t;

extern void gui_set_mode(gui_mode_t idx);
extern gui_mode_t gui_get_mode(void);

extern void gui_init(void);

#endif /* INC_USER_GUI_H_ */

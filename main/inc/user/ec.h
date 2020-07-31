/*
 * ec.h
 *
 *  Created on: 2020-05-25 13:32
 *      Author: Jack Chen <redchenjs@live.com>
 */

#ifndef INC_USER_EC_H_
#define INC_USER_EC_H_

typedef enum {
    EC_EVT_N   = 0x0,
    EC_EVT_I   = 0x1,
    EC_EVT_D   = 0x2,

    EC_EVT_N_B = 0x3,
    EC_EVT_I_B = 0x4,
    EC_EVT_D_B = 0x5,

    EC_EVT_MAX
} encoder_evt_t;

extern void ec_init(void);

#endif /* INC_USER_EC_H_ */

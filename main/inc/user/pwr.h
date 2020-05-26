/*
 * pwr.h
 *
 *  Created on: 2020-05-25 13:39
 *      Author: Jack Chen <redchenjs@live.com>
 */

#ifndef INC_USER_PWR_H_
#define INC_USER_PWR_H_

typedef enum {
    PWR_IDX_DC     = 0x0,
    PWR_IDX_SDP    = 0x1,
    PWR_IDX_DCP    = 0x2,

    PWR_IDX_QC_5V  = 0x3,
    PWR_IDX_QC_9V  = 0x4,
    PWR_IDX_QC_12V = 0x5,

    PWR_IDX_MAX
} pwr_idx_t;

extern pwr_idx_t pwr_get_mode(void);
extern char *pwr_get_mode_str(void);

extern void pwr_init(pwr_idx_t idx);

#endif /* INC_USER_PWR_H_ */

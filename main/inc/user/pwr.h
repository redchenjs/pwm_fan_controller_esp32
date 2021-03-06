/*
 * pwr.h
 *
 *  Created on: 2020-05-25 13:39
 *      Author: Jack Chen <redchenjs@live.com>
 */

#ifndef INC_USER_PWR_H_
#define INC_USER_PWR_H_

typedef enum {
    PWR_MODE_IDX_DC     = 0x00,
    PWR_MODE_IDX_SDP    = 0x01,
    PWR_MODE_IDX_DCP    = 0x02,
    PWR_MODE_IDX_QC_5V  = 0x03,
    PWR_MODE_IDX_QC_9V  = 0x04,
    PWR_MODE_IDX_QC_12V = 0x05,

    PWR_MODE_IDX_MAX
} pwr_mode_t;

extern void pwr_set_mode(pwr_mode_t idx);

extern pwr_mode_t pwr_get_mode(void);
extern char *pwr_get_mode_str(void);

extern void pwr_env_save(void);
extern bool pwr_env_saved(void);

extern void pwr_deinit(void);
extern void pwr_init(void);

#endif /* INC_USER_PWR_H_ */

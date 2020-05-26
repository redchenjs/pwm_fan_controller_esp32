/*
 * qc.h
 *
 *  Created on: 2020-05-25 13:39
 *      Author: Jack Chen <redchenjs@live.com>
 */

#ifndef INC_USER_QC_H_
#define INC_USER_QC_H_

typedef enum {
    QC_IDX_DC  = 0x0,
    QC_IDX_SDP = 0x1,
    QC_IDX_DCP = 0x2,

    QC_IDX_5V  = 0x3,
    QC_IDX_9V  = 0x4,
    QC_IDX_12V = 0x5,

    QC_IDX_MAX
} qc_idx_t;

extern qc_idx_t qc_get_mode(void);
extern char *qc_get_mode_str(void);

extern void qc_init(qc_idx_t idx);

#endif /* INC_USER_QC_H_ */

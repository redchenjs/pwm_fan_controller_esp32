/*
 * i2c.h
 *
 *  Created on: 2019-04-18 20:26
 *      Author: Jack Chen <redchenjs@live.com>
 */

#ifndef INC_CHIP_I2C_H_
#define INC_CHIP_I2C_H_

#include "driver/i2c.h"

#define I2C_HOST_TAG "i2c-0"
#define I2C_HOST_NUM I2C_NUM_0

extern void i2c_host_init(void);

#endif /* INC_CHIP_I2C_H_ */

/*
 * ina219.h
 *
 *  Created on: 2020-05-29 10:58
 *      Author: Jack Chen <redchenjs@live.com>
 */

#ifndef INC_BOARD_INA219_H_
#define INC_BOARD_INA219_H_

extern void ina219_set_calibration_32v_2a(void);
extern void ina219_set_calibration_32v_1a(void);
extern void ina219_set_calibration_16v_400ma(void);

extern float ina219_get_shunt_voltage_mv(void);
extern float ina219_get_bus_voltage_v(void);
extern float ina219_get_current_ma(void);
extern float ina219_get_power_mw(void);

extern void ina219_init(void);

#endif /* INC_BOARD_INA219_H_ */

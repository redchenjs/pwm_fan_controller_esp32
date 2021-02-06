/*
 * ina219.c
 *
 *  Created on: 2020-05-29 10:58
 *      Author: Jack Chen <redchenjs@live.com>
 */

#include <string.h>

#include "esp_log.h"

#include "driver/i2c.h"

#define TAG "ina219"

#define INA219_ADDR               0x40    // A0 + A1 = GND

#define INA219_REG_CONFIG         0x00
#define INA219_REG_SHUNT_VOLTAGE  0x01
#define INA219_REG_BUS_VOLTAGE    0x02
#define INA219_REG_POWER          0x03
#define INA219_REG_CURRENT        0x04
#define INA219_REG_CALIBRATION    0x05

typedef union {
    struct {
        uint8_t mode:     3;
        uint8_t sadc:     4;
        uint8_t badc:     4;
        uint8_t pg:       2;
        uint8_t brng:     1;
        uint8_t reserved: 1;
        uint8_t rst:      1;
    };
    uint16_t val;
} ina219_conf_t;

enum brng_settings {
    BRNG_16V_FSR = 0x0,
    BRNG_32V_FSR = 0x1
};

enum pga_settings {
    PGA_GAIN_1_40MV  = 0x0,
    PGA_GAIN_2_80MV  = 0x1,
    PGA_GAIN_4_160MV = 0x2,
    PGA_GAIN_8_320MV = 0x3
};

enum adc_settings {
    ADC_RES_9BIT       = 0x0,
    ADC_RES_10BIT      = 0x1,
    ADC_RES_11BIT      = 0x2,
    ADC_RES_12BIT      = 0x3,
    ADC_RES_12BIT_2S   = 0x9,
    ADC_RES_12BIT_4S   = 0xA,
    ADC_RES_12BIT_8S   = 0xB,
    ADC_RES_12BIT_16S  = 0xC,
    ADC_RES_12BIT_32S  = 0xD,
    ADC_RES_12BIT_64S  = 0xE,
    ADC_RES_12BIT_128S = 0xF
};

enum mode_settings {
    MODE_POWER_DOWN               = 0x0,
    MODE_SHUNT_VOLTAGE_TRIGGERED  = 0x1,
    MODE_BUS_VOLTAGE_TRIGGERED    = 0x2,
    MODE_SHUNT_AND_BUS_TRIGGERED  = 0x3,
    MODE_ADC_OFF                  = 0x4,
    MODE_SHUNT_VOLTAGE_CONTINUOUS = 0x5,
    MODE_BUS_VOLTAGE_CONTINUOUS   = 0x6,
    MODE_SHUNT_AND_BUS_CONTINUOUS = 0x7
};

static uint16_t ina219_cal_val = 0;
static uint16_t ina219_cur_div = 1;
static float    ina219_pwr_mul = 0.0;

static ina219_conf_t ina219_conf = {0};

static void ina219_reg_write(uint8_t reg, const uint8_t *buff)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);

    i2c_master_write_byte(cmd, INA219_ADDR << 1 | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, buff[1], true);
    i2c_master_write_byte(cmd, buff[0], true);

    i2c_master_stop(cmd);

    i2c_master_cmd_begin(I2C_NUM_0, cmd, portMAX_DELAY);

    i2c_cmd_link_delete(cmd);
}

#ifdef CONFIG_ENABLE_POWER_MONITOR
static void ina219_reg_read(uint8_t reg, uint8_t *buff)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);

    i2c_master_write_byte(cmd, INA219_ADDR << 1 | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);

    i2c_master_start(cmd);

    i2c_master_write_byte(cmd, INA219_ADDR << 1 | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &buff[1], I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &buff[0], I2C_MASTER_NACK);

    i2c_master_stop(cmd);

    i2c_master_cmd_begin(I2C_NUM_0, cmd, portMAX_DELAY);

    i2c_cmd_link_delete(cmd);
}
#endif

static int16_t ina219_get_shunt_voltage_raw(void)
{
    int16_t value = 0;

#ifdef CONFIG_ENABLE_POWER_MONITOR
    ina219_reg_read(INA219_REG_SHUNT_VOLTAGE, (uint8_t *)&value);
#endif

    return value;
}

static int16_t ina219_get_bus_voltage_raw(void)
{
    int16_t value = 0;

#ifdef CONFIG_ENABLE_POWER_MONITOR
    ina219_reg_read(INA219_REG_BUS_VOLTAGE, (uint8_t *)&value);
#endif

    return ((value >> 3) * 4);
}

static int16_t ina219_get_current_raw(void)
{
    int16_t value = 0;

#ifdef CONFIG_ENABLE_POWER_MONITOR
    ina219_reg_write(INA219_REG_CALIBRATION, (const uint8_t *)&ina219_cal_val);
    ina219_reg_read(INA219_REG_CURRENT, (uint8_t *)&value);
#endif

    return value;
}

static int16_t ina219_get_power_raw(void)
{
    int16_t value = 0;

#ifdef CONFIG_ENABLE_POWER_MONITOR
    ina219_reg_write(INA219_REG_CALIBRATION, (const uint8_t *)&ina219_cal_val);
    ina219_reg_read(INA219_REG_POWER, (uint8_t *)&value);
#endif

    return value;
}

void ina219_set_calibration_32v_2a(void)
{
    ina219_cal_val = 4096;      // 3.2A max
    ina219_cur_div = 10;        // 100uA per bit
    ina219_pwr_mul = 2.0;       // 1mW per bit

    ina219_conf.rst  = 0;
    ina219_conf.brng = BRNG_32V_FSR;
    ina219_conf.pg   = PGA_GAIN_8_320MV;
    ina219_conf.badc = ADC_RES_12BIT_128S;
    ina219_conf.sadc = ADC_RES_12BIT_128S;
    ina219_conf.mode = MODE_SHUNT_AND_BUS_CONTINUOUS;

    ina219_reg_write(INA219_REG_CALIBRATION, (const uint8_t *)&ina219_cal_val);
    ina219_reg_write(INA219_REG_CONFIG, (const uint8_t *)&ina219_conf);
}

void ina219_set_calibration_32v_1a(void)
{
    ina219_cal_val = 10240;     // 1.3A max
    ina219_cur_div = 25;        // 40uA per bit
    ina219_pwr_mul = 0.8;       // 800uW per bit

    ina219_conf.rst  = 0;
    ina219_conf.brng = BRNG_32V_FSR;
    ina219_conf.pg   = PGA_GAIN_8_320MV;
    ina219_conf.badc = ADC_RES_12BIT_128S;
    ina219_conf.sadc = ADC_RES_12BIT_128S;
    ina219_conf.mode = MODE_SHUNT_AND_BUS_CONTINUOUS;

    ina219_reg_write(INA219_REG_CALIBRATION, (const uint8_t *)&ina219_cal_val);
    ina219_reg_write(INA219_REG_CONFIG, (const uint8_t *)&ina219_conf);
}

void ina219_set_calibration_16v_400ma(void)
{
    ina219_cal_val = 8192;      // 400mA max
    ina219_cur_div = 20;        // 50uA per bit
    ina219_pwr_mul = 1.0;       // 1mW per bit

    ina219_conf.rst  = 0;
    ina219_conf.brng = BRNG_16V_FSR;
    ina219_conf.pg   = PGA_GAIN_1_40MV;
    ina219_conf.badc = ADC_RES_12BIT_128S;
    ina219_conf.sadc = ADC_RES_12BIT_128S;
    ina219_conf.mode = MODE_SHUNT_AND_BUS_CONTINUOUS;

    ina219_reg_write(INA219_REG_CALIBRATION, (const uint8_t *)&ina219_cal_val);
    ina219_reg_write(INA219_REG_CONFIG, (const uint8_t *)&ina219_conf);
}

float ina219_get_shunt_voltage_mv(void)
{
    return (ina219_get_shunt_voltage_raw() * 0.01);
}

float ina219_get_bus_voltage_mv(void)
{
    return ina219_get_bus_voltage_raw();
}

float ina219_get_current_ma(void)
{
    return (ina219_get_current_raw() / ina219_cur_div);
}

float ina219_get_power_mw(void)
{
    return (ina219_get_power_raw() * ina219_pwr_mul);
}

void ina219_init(void)
{
    ina219_set_calibration_32v_2a();

    ESP_LOGI(TAG, "initialized.");
}

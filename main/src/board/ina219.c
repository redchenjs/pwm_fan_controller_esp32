/*
 * ina219.c
 *
 *  Created on: 2020-05-29 10:58
 *      Author: Jack Chen <redchenjs@live.com>
 */

#include "esp_log.h"

#include "driver/i2c.h"
#include "board/ina219.h"

#define TAG "ina219"

#define INA219_ADDR               0x40    // A0 + A1 = GND

#define INA219_REG_CONFIG         0x00
#define INA219_REG_SHUNT_VOLTAGE  0x01
#define INA219_REG_BUS_VOLTAGE    0x02
#define INA219_REG_POWER          0x03
#define INA219_REG_CURRENT        0x04
#define INA219_REG_CALIBRATION    0x05

#define INA219_CONFIG_RESET_BIT   0x8000

enum bus_voltage_range_settings {
    INA219_CONFIG_BUS_VOLTAGE_RANGE_16V = 0x0000,
    INA219_CONFIG_BUS_VOLTAGE_RANGE_32V = 0x2000,
};

enum pg_bit_settings {
    INA219_CONFIG_GAIN_1_40MV  = 0x0000,
    INA219_CONFIG_GAIN_2_80MV  = 0x0800,
    INA219_CONFIG_GAIN_4_160MV = 0x1000,
    INA219_CONFIG_GAIN_8_320MV = 0x1800,
};

enum bus_adc_settings {
    INA219_CONFIG_BUS_ADC_RES_9BIT             = 0x0000,
    INA219_CONFIG_BUS_ADC_RES_10BIT            = 0x0080,
    INA219_CONFIG_BUS_ADC_RES_11BIT            = 0x0100,
    INA219_CONFIG_BUS_ADC_RES_12BIT            = 0x0180,
    INA219_CONFIG_BUS_ADC_RES_12BIT_2S_1060US  = 0x0480,
    INA219_CONFIG_BUS_ADC_RES_12BIT_4S_2130US  = 0x0500,
    INA219_CONFIG_BUS_ADC_RES_12BIT_8S_4260US  = 0x0580,
    INA219_CONFIG_BUS_ADC_RES_12BIT_16S_8510US = 0x0600,
    INA219_CONFIG_BUS_ADC_RES_12BIT_32S_17MS   = 0x0680,
    INA219_CONFIG_BUS_ADC_RES_12BIT_64S_34MS   = 0x0700,
    INA219_CONFIG_BUS_ADC_RES_12BIT_128S_69MS  = 0x0780,
};

enum shunt_adc_settings {
    INA219_CONFIG_SHUNT_ADC_RES_9BIT_1S_84US     = 0x0000,
    INA219_CONFIG_SHUNT_ADC_RES_10BIT_1S_148US   = 0x0008,
    INA219_CONFIG_SHUNT_ADC_RES_11BIT_1S_276US   = 0x0010,
    INA219_CONFIG_SHUNT_ADC_RES_12BIT_1S_532US   = 0x0018,
    INA219_CONFIG_SHUNT_ADC_RES_12BIT_2S_1060US  = 0x0048,
    INA219_CONFIG_SHUNT_ADC_RES_12BIT_4S_2130US  = 0x0050,
    INA219_CONFIG_SHUNT_ADC_RES_12BIT_8S_4260US  = 0x0058,
    INA219_CONFIG_SHUNT_ADC_RES_12BIT_16S_8510US = 0x0060,
    INA219_CONFIG_SHUNT_ADC_RES_12BIT_32S_17MS   = 0x0068,
    INA219_CONFIG_SHUNT_ADC_RES_12BIT_64S_34MS   = 0x0070,
    INA219_CONFIG_SHUNT_ADC_RES_12BIT_128S_69MS  = 0x0078,
};

enum mode_settings {
    INA219_CONFIG_MODE_POWER_DOWN                       = 0x00,
    INA219_CONFIG_MODE_SHUNT_VOLTAGE_TRIGGERED          = 0x01,
    INA219_CONFIG_MODE_BUS_VOLTAGE_TRIGGERED            = 0x02,
    INA219_CONFIG_MODE_SHUNT_AND_BUS_VOLTAGE_TRIGGERED  = 0x03,
    INA219_CONFIG_MODE_ADC_OFF                          = 0x04,
    INA219_CONFIG_MODE_SHUNT_VOLTAGE_CONTINUOUS         = 0x05,
    INA219_CONFIG_MODE_BUS_VOLTAGE_CONTINUOUS           = 0x06,
    INA219_CONFIG_MODE_SHUNT_AND_BUS_VOLTAGE_CONTINUOUS = 0x07,
};

static uint16_t ina219_conf = 0;
static uint16_t ina219_cal_val = 0;
static uint16_t ina219_curr_div = 0;
static float ina219_power_mul = 0.0;

static void ina219_write(uint8_t reg, const uint8_t *buff)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);

    i2c_master_write_byte(cmd, INA219_ADDR << 1 | I2C_MASTER_WRITE, 1);
    i2c_master_write_byte(cmd, reg, 1);
    i2c_master_write_byte(cmd, buff[1], 1);
    i2c_master_write_byte(cmd, buff[0], 1);

    i2c_master_stop(cmd);

    i2c_master_cmd_begin(I2C_NUM_0, cmd, portMAX_DELAY);

    i2c_cmd_link_delete(cmd);
}

#ifdef CONFIG_ENABLE_POWER_MONITOR
static void ina219_read(uint8_t reg, uint8_t *buff)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);

    i2c_master_write_byte(cmd, INA219_ADDR << 1 | I2C_MASTER_WRITE, 1);
    i2c_master_write_byte(cmd, reg, 1);

    i2c_master_start(cmd);

    i2c_master_write_byte(cmd, INA219_ADDR << 1 | I2C_MASTER_READ, 1);
    i2c_master_read_byte(cmd, &buff[1], 0);
    i2c_master_read_byte(cmd, &buff[0], 1);

    i2c_master_stop(cmd);

    i2c_master_cmd_begin(I2C_NUM_0, cmd, portMAX_DELAY);

    i2c_cmd_link_delete(cmd);
}
#endif

static int16_t ina219_get_bus_voltage_raw(void)
{
    int16_t value = 0;

#ifdef CONFIG_ENABLE_POWER_MONITOR
    ina219_read(INA219_REG_BUS_VOLTAGE, (uint8_t *)&value);
#endif

    return ((value >> 3) * 4);
}

static int16_t ina219_get_shunt_voltage_raw(void)
{
    int16_t value = 0;

#ifdef CONFIG_ENABLE_POWER_MONITOR
    ina219_read(INA219_REG_SHUNT_VOLTAGE, (uint8_t *)&value);
#endif

    return value;
}

static int16_t ina219_get_current_raw(void)
{
    int16_t value = 0;

#ifdef CONFIG_ENABLE_POWER_MONITOR
    ina219_write(INA219_REG_CALIBRATION, (const uint8_t *)&ina219_cal_val);
    ina219_read(INA219_REG_CURRENT, (uint8_t *)&value);
#endif

    return value;
}

static int16_t ina219_get_power_raw(void)
{
    int16_t value = 0;

#ifdef CONFIG_ENABLE_POWER_MONITOR
    ina219_write(INA219_REG_CALIBRATION, (const uint8_t *)&ina219_cal_val);
    ina219_read(INA219_REG_POWER, (uint8_t *)&value);
#endif

    return value;
}

void ina219_set_calibration_32v_2a(void)
{
    ina219_conf = INA219_CONFIG_BUS_VOLTAGE_RANGE_32V |
                  INA219_CONFIG_GAIN_8_320MV |
                  INA219_CONFIG_BUS_ADC_RES_12BIT_128S_69MS |
                  INA219_CONFIG_SHUNT_ADC_RES_12BIT_128S_69MS |
                  INA219_CONFIG_MODE_SHUNT_AND_BUS_VOLTAGE_CONTINUOUS;
    ina219_cal_val = 4096;      // 3.2A max
    ina219_curr_div = 10;       // 100uA per bit
    ina219_power_mul = 2.0;     // 1mW per bit

    ina219_write(INA219_REG_CALIBRATION, (const uint8_t *)&ina219_cal_val);
    ina219_write(INA219_REG_CONFIG, (const uint8_t *)&ina219_conf);
}

void ina219_set_calibration_32v_1a(void)
{
    ina219_conf = INA219_CONFIG_BUS_VOLTAGE_RANGE_32V |
                  INA219_CONFIG_GAIN_8_320MV |
                  INA219_CONFIG_BUS_ADC_RES_12BIT_128S_69MS |
                  INA219_CONFIG_SHUNT_ADC_RES_12BIT_128S_69MS |
                  INA219_CONFIG_MODE_SHUNT_AND_BUS_VOLTAGE_CONTINUOUS;
    ina219_cal_val = 10240;     // 1.3A max
    ina219_curr_div = 25;       // 40uA per bit
    ina219_power_mul = 0.8;     // 800uW per bit

    ina219_write(INA219_REG_CALIBRATION, (const uint8_t *)&ina219_cal_val);
    ina219_write(INA219_REG_CONFIG, (const uint8_t *)&ina219_conf);
}

void ina219_set_calibration_16v_400ma(void)
{
    ina219_conf = INA219_CONFIG_BUS_VOLTAGE_RANGE_16V |
                  INA219_CONFIG_GAIN_1_40MV |
                  INA219_CONFIG_BUS_ADC_RES_12BIT_128S_69MS |
                  INA219_CONFIG_SHUNT_ADC_RES_12BIT_128S_69MS |
                  INA219_CONFIG_MODE_SHUNT_AND_BUS_VOLTAGE_CONTINUOUS;
    ina219_cal_val = 8192;      // 400mA max
    ina219_curr_div = 20;       // 50uA per bit
    ina219_power_mul = 1.0;     // 1mW per bit

    ina219_write(INA219_REG_CALIBRATION, (const uint8_t *)&ina219_cal_val);
    ina219_write(INA219_REG_CONFIG, (const uint8_t *)&ina219_conf);
}

float ina219_get_shunt_voltage_mv(void)
{
    return (ina219_get_shunt_voltage_raw() * 0.01);
}

float ina219_get_bus_voltage_v(void)
{
    return (ina219_get_bus_voltage_raw() * 0.001);
}

float ina219_get_current_ma(void)
{
    return (ina219_get_current_raw() / ina219_curr_div);
}

float ina219_get_power_mw(void)
{
    return (ina219_get_power_raw() * ina219_power_mul);
}

void ina219_init(void)
{
    ina219_conf = INA219_CONFIG_RESET_BIT;
    ina219_write(INA219_REG_CONFIG, (const uint8_t *)&ina219_conf);

    ina219_set_calibration_32v_2a();

    ESP_LOGI(TAG, "initialized.");
}

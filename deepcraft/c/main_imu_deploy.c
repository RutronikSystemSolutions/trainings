/******************************************************************************
* File Name:   main.c
*
* Description: Implementation of Tensor Streaming Protocol firmware for PSOC 6.
*
* Related Document:
*    See README.md and https://bitbucket.org/imagimob/tensor-streaming-protocol
*
*******************************************************************************
* Copyright 2024-2025, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <cyhal.h>
#include <stdio.h>
#include <cybsp.h>

#include "protocol/protocol.h"
#include "usbd.h"
#include "build.h"
#include "common.h"
#include "clock.h"
#include "board.h"
#include "system.h"
#include "watchdog.h"

#include <bmi270.h>
#include "dev_bmi270.h"

// Add those modules in your makefile
// COMPONENTS+=ML_TFLM ML_FLOAT32
// DEFINES+=TF_LITE_STATIC_MEMORY

#define _I2C_TIMEOUT_MS            (10U)
#define _READ_WRITE_LEN            (46U)
#define _SOFT_RESET_DELAY_US       (300)

#ifdef IM_XSS_BMI270
#define BMI270_ADDRESS (BMI2_I2C_SEC_ADDR)
#else
#define BMI270_ADDRESS (BMI2_I2C_PRIM_ADDR)
#endif

/* Earth's gravity in m/s^2 */
#define GRAVITY_EARTH  (9.80665f)

#define SENSOR_COUNT 2

typedef struct {
    /* Hardware device */
    struct bmi2_dev sensor;

    /* Accelerator range in G. Valid values are: 2,4,8,16 */
    float accel_range;

    /* Gyroscope range in DPS. Valid values are: 125,250,500,1000,2000 */
    float gyro_range;

    /* Tick of last sample */
    clock_tick_t sample_time_tick;
    /* Used to ensure the first read */
    bool first_sample;

    /* The sample period in ticks */
    uint32_t period_tick;

    float data_combined[6];
} dev_bmi270_t;

static cyhal_i2c_t i2c;
static dev_bmi270_t bmi270_dev;

static bool _init_i2c(cyhal_i2c_t* i2c)
{
    cy_rslt_t result;

    /* I2C config structure */
    cyhal_i2c_cfg_t i2c_config =
    {
         .is_slave = false,
         .address = 0,
         .frequencyhal_hz = 400000
    };

    /* Initialize I2C for IMU communication */
    result = cyhal_i2c_init(i2c, CYBSP_I2C_SDA, CYBSP_I2C_SCL, NULL);
    if(result != CY_RSLT_SUCCESS)
    {
        return false;
    }

    /* Configure the I2C */
    result = cyhal_i2c_configure(i2c, &i2c_config);
    if(result != CY_RSLT_SUCCESS)
    {
        return false;
    }

    return true;
}

static BMI2_INTF_RETURN_TYPE _bmi2_i2c_read(
        uint8_t reg_addr,
        uint8_t* reg_data,
        uint32_t len,
        void* intf_ptr)
{
    cyhal_i2c_t *i2c = (cyhal_i2c_t*)intf_ptr;

    return (BMI2_INTF_RETURN_TYPE)cyhal_i2c_master_mem_read(
        i2c,
        BMI270_ADDRESS,
        reg_addr,
        1,
        reg_data,
        (uint16_t)len,
        _I2C_TIMEOUT_MS);
}

static BMI2_INTF_RETURN_TYPE _bmi2_i2c_write(
        uint8_t reg_addr,
        const uint8_t* reg_data,
        uint32_t len,
        void* intf_ptr)
{
    cyhal_i2c_t *i2c = (cyhal_i2c_t*)intf_ptr;

    return (BMI2_INTF_RETURN_TYPE)cyhal_i2c_master_mem_write(
        i2c,
        BMI270_ADDRESS,
        reg_addr,
        1,
        reg_data,
        (uint16_t)len,
        _I2C_TIMEOUT_MS);
}

static void _bmi2_delay_us(uint32_t us, void* intf_ptr)
{
    UNUSED(intf_ptr);

    cyhal_system_delay_us(us);
}

static bool _init_hw(dev_bmi270_t *dev, cyhal_i2c_t* i2c)
{
    dev->sensor.intf = BMI2_I2C_INTF;
    dev->sensor.read = _bmi2_i2c_read;
    dev->sensor.write = _bmi2_i2c_write;
    dev->sensor.delay_us = _bmi2_delay_us;
    dev->sensor.intf_ptr = i2c;
    dev->sensor.read_write_len = _READ_WRITE_LEN;
    dev->sensor.config_file_ptr = NULL;

    if(bmi270_init(&(dev->sensor)) != BMI2_OK)
    {
        return false;
    }

    printf("bmi270: Initialized device.\r\n");

    return true;
}

static float _lsb_to_dps(int16_t val, float dps, uint8_t bit_width)
{
    double power = 2;

    float half_scale = (float)((pow((double)power, (double)bit_width) / 2.0f));

    return (dps / (half_scale)) * (val);
}

static float _lsb_to_mps2(int16_t val, float g_range, uint8_t bit_width)
{
    double power = 2;

    float half_scale = (float)((pow((double)power, (double)bit_width) / 2.0f));

    return (GRAVITY_EARTH * val * g_range) / half_scale;
}

static bool _read_hw(dev_bmi270_t* dev)
{
    int8_t result;
    struct bmi2_sens_data data = { { 0 } };
    struct bmi2_dev *sensor = &dev->sensor;

    result = bmi2_get_sensor_data(&data, sensor);

    if(result != BMI2_OK)
    {
        return false;
    }

	if(!(data.status & BMI2_DRDY_ACC) || !(data.status & BMI2_DRDY_GYR))
		return false;

	float *dest = dev->data_combined;
	*dest++ = _lsb_to_mps2(data.acc.x, dev->accel_range, sensor->resolution);
	*dest++ = _lsb_to_mps2(data.acc.y, dev->accel_range, sensor->resolution);
	*dest++ = _lsb_to_mps2(data.acc.z, dev->accel_range, sensor->resolution);
	*dest++ = _lsb_to_dps(data.gyr.x, dev->gyro_range, sensor->resolution);
	*dest++ = _lsb_to_dps(data.gyr.y, dev->gyro_range, sensor->resolution);
	*dest++ = _lsb_to_dps(data.gyr.z, dev->gyro_range, sensor->resolution);
    return true;
}

static bool _config_hw(dev_bmi270_t* dev, int rate, int accel_range, int gyro_range)
{
    int8_t result;

    dev->sample_time_tick = 0;
    dev->first_sample = true;
    dev->period_tick = CLOCK_TICK_PER_SECOND / rate;
    dev->accel_range = accel_range;
    dev->gyro_range = gyro_range;

    struct bmi2_sens_config config[SENSOR_COUNT];
    config[0].type = BMI2_ACCEL;
    config[1].type = BMI2_GYRO;

    /* Get sensor configuration */
    result = bmi2_get_sensor_config(config, SENSOR_COUNT, &(dev->sensor));
    if (BMI2_OK != result)
    {
        return result;
    }

    /* Set output data rate and range based on rate and range parameters */
    switch(rate) {
        case 25:
            config[0].cfg.acc.odr = BMI2_ACC_ODR_25HZ;
            config[1].cfg.gyr.odr = BMI2_GYR_ODR_25HZ;
            break;
        case 50:
            config[0].cfg.acc.odr = BMI2_ACC_ODR_50HZ;
            config[1].cfg.gyr.odr = BMI2_GYR_ODR_50HZ;
            break;
        case 100:
            config[0].cfg.acc.odr = BMI2_ACC_ODR_100HZ;
            config[1].cfg.gyr.odr = BMI2_GYR_ODR_100HZ;
            break;
        case 200:
            config[0].cfg.acc.odr = BMI2_ACC_ODR_200HZ;
            config[1].cfg.gyr.odr = BMI2_GYR_ODR_200HZ;
            break;
        case 400:
            config[0].cfg.acc.odr = BMI2_ACC_ODR_400HZ;
            config[1].cfg.gyr.odr = BMI2_GYR_ODR_400HZ;
            break;
        case 800:
            config[0].cfg.acc.odr = BMI2_ACC_ODR_800HZ;
            config[1].cfg.gyr.odr = BMI2_GYR_ODR_800HZ;
            break;
        default: return false;
    }

    switch(accel_range) {
        case 2: config[0].cfg.acc.range = BMI2_ACC_RANGE_2G; break;
        case 4: config[0].cfg.acc.range = BMI2_ACC_RANGE_4G; break;
        case 8: config[0].cfg.acc.range = BMI2_ACC_RANGE_8G; break;
        case 16: config[0].cfg.acc.range = BMI2_ACC_RANGE_16G; break;
        default: return false;
    }

    switch(gyro_range) {
        case 125: config[1].cfg.gyr.range = BMI2_GYR_RANGE_125; break;
        case 250: config[1].cfg.gyr.range = BMI2_GYR_RANGE_250; break;
        case 500: config[1].cfg.gyr.range = BMI2_GYR_RANGE_500; break;
        case 1000: config[1].cfg.gyr.range = BMI2_GYR_RANGE_1000; break;
        case 2000: config[1].cfg.gyr.range = BMI2_GYR_RANGE_2000; break;
        default: return false;
    }

    config[0].type = BMI2_ACCEL;
    config[1].type = BMI2_GYRO;

    /* The bandwidth parameter is used to configure the number of sensor samples that are averaged
     * if it is set to 2, then 2^(bandwidth parameter) samples
     * are averaged, resulting in 4 averaged samples
     * Note1 : For more information, refer the datasheet.
     * Note2 : A higher number of averaged samples will result in a lower noise level of the signal, but
     * this has an adverse effect on the power consumed.
     */
    config[0].cfg.acc.bwp = BMI2_ACC_OSR4_AVG1;

    /* Enable the filter performance mode where averaging of samples
     * will be done based on above set bandwidth and ODR.
     * There are two modes
     *  0 -> Ultra low power mode
     *  1 -> High performance mode(Default)
     * For more info refer datasheet.
     */
    config[0].cfg.acc.filter_perf = BMI2_PERF_OPT_MODE;

    /* Gyroscope Bandwidth parameters. By default the gyro bandwidth is in normal mode. */
    config[1].cfg.gyr.bwp = BMI2_GYR_NORMAL_MODE;

    /* Enable/Disable the noise performance mode for precision yaw rate sensing
     * There are two modes
     *  0 -> Ultra low power mode(Default)
     *  1 -> High performance mode
     */
    config[1].cfg.gyr.noise_perf = BMI2_POWER_OPT_MODE;

    /* Enable/Disable the filter performance mode where averaging of samples
     * will be done based on above set bandwidth and ODR.
     * There are two modes
     *  0 -> Ultra low power mode
     *  1 -> High performance mode(Default)
     */
    config[1].cfg.gyr.filter_perf = BMI2_PERF_OPT_MODE;

    /* Configure the sensors */
    result = bmi2_set_sensor_config(config, SENSOR_COUNT, &(dev->sensor));
    if (result != BMI2_OK)
    {
        return result;
    }

    /* Enable the sensors */
    uint8_t sens_list[SENSOR_COUNT] = { BMI2_ACCEL, BMI2_GYRO };
    result = bmi2_sensor_enable(sens_list, 2, &(dev->sensor));

    if (result != BMI2_OK)
    {
        return result;
    }

    /* It takes a while for the sensor to deliver data.
     * Wait for 5 successful reads. Give up after 2 seconds.
     */
    int success = 0;
    for(int i = 0; i < 20; i++)
    {
        if(_read_hw(dev))
            success++;        /* We got data! */

        if(success >= 5) {
        	printf("BMI 270 configured\r\n");
            return true;    /* OK, all good. */
        }

        cyhal_system_delay_ms(100);
    }

   return false;
}

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
*  This is the main function. It never returns.
*
*******************************************************************************/
int main(void)
{
    /* Base system initialization  */
    board_init_system();

    /* Override the base PLL clocks and routing. */
    board_set_clocks();

    /* Start clock */
    if(!clock_init())
    {
        halt_error(LED_CODE_CLOCK_ERROR);
    }

    /* Initialize retarget-io to use the debug UART port */
    if(!board_enable_debug_console())
    {
        halt_error(LED_CODE_STDOUT_RETARGET_ERROR);
    }

    printf("IMU Deploy Project\r\n");

    if (! _init_i2c(&i2c))
    {
    	printf("Cannot init i2c\r\n");
    	return 0;
    }

    if (! _init_hw(&bmi270_dev, &i2c))
    {
    	printf("Cannot init bmi 270\r\n");
    	return 0;
    }

    if ( ! _config_hw(&bmi270_dev, 50, 2 , 500))
    {
    	printf("Cannot configure bmi 270\r\n");
    	return 0;
    }

    for(;;)
    {
    	if (_read_hw(&bmi270_dev))
    	{
    		printf("Val = %f\r\n", bmi270_dev.data_combined[0]);
    	}
    }
}

/* [] END OF FILE */

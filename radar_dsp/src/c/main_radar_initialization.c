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
#include "cycfg_pins.h"

#include "protocol/protocol.h"
#include "usbd.h"
#include "build.h"
#include "common.h"
#include "clock.h"
#include "board.h"
#include "system.h"
#include "watchdog.h"

// Access to the BGT60TR13C API (configure, start frame, ...)
#include <xensiv_bgt60trxx_mtb.h>

// Access to the radar settings exported using the Radar Fusion GUI
#define XENSIV_BGT60TRXX_CONF_IMPL
#include "radar_settings.h"

// Compute how many samples a frame contains
#define NUM_SAMPLES_PER_FRAME (XENSIV_BGT60TRXX_CONF_NUM_SAMPLES_PER_CHIRP * XENSIV_BGT60TRXX_CONF_NUM_CHIRPS_PER_FRAME * XENSIV_BGT60TRXX_CONF_NUM_RX_ANTENNAS)

static cyhal_spi_t spi;
static xensiv_bgt60trxx_mtb_t bgt60_obj;

static uint16_t data_available = 0;

/**
 * @brief Interrupt service routine called when radar values (a frame) are available
 *
 * See xensiv_bgt60trxx_mtb_interrupt_init
 */
void radar_isr(void *args, cyhal_gpio_event_t event)
{
    CY_UNUSED_PARAMETER(args);
    CY_UNUSED_PARAMETER(event);

    // Values are available, then can be read using the function xensiv_bgt60trxx_get_fifo_data
    data_available = 1;

    printf("isr called\r\n");
}

/**
 * @brief Initialize the SPI block that will be used for the communication
 * with the radar sensor
 *
 * @retval true on success
 */
static bool _init_spi(cyhal_spi_t* spi)
{
    cy_rslt_t result = cyhal_spi_init(
            spi,
            CYBSP_RSPI_MOSI,
            CYBSP_RSPI_MISO,
            CYBSP_RSPI_CLK,
            NC,
            NULL,
            8,
            CYHAL_SPI_MODE_00_MSB,
            false);
    if(result != CY_RSLT_SUCCESS)
    {
        return false;
    }

    result = cyhal_spi_set_frequency(spi, 18750000UL);

    if(result != CY_RSLT_SUCCESS)
    {
        return false;
    }

    return true;
}

/**
 * @brief Initialize the radar sensor (without starting it)
 *
 * @retval true on success
 */
static bool _init_bgt60tr13c()
{
    cy_rslt_t result;

    /* Reduce drive strength to improve EMI */
    Cy_GPIO_SetSlewRate(CYHAL_GET_PORTADDR(CYBSP_RSPI_MOSI), CYHAL_GET_PIN(CYBSP_RSPI_MOSI), CY_GPIO_SLEW_FAST);
    Cy_GPIO_SetDriveSel(CYHAL_GET_PORTADDR(CYBSP_RSPI_MOSI), CYHAL_GET_PIN(CYBSP_RSPI_MOSI), CY_GPIO_DRIVE_1_8);
    Cy_GPIO_SetSlewRate(CYHAL_GET_PORTADDR(CYBSP_RSPI_CLK), CYHAL_GET_PIN(CYBSP_RSPI_CLK), CY_GPIO_SLEW_FAST);
    Cy_GPIO_SetDriveSel(CYHAL_GET_PORTADDR(CYBSP_RSPI_CLK), CYHAL_GET_PIN(CYBSP_RSPI_CLK), CY_GPIO_DRIVE_1_8);

    /* Initialization uses preset 0 */
    result = xensiv_bgt60trxx_mtb_init(
            &bgt60_obj,
            &spi,
            CYBSP_RSPI_CS,
            CYBSP_RXRES_L,
            register_list,
			XENSIV_BGT60TRXX_CONF_NUM_REGS
            );

    if (result != CY_RSLT_SUCCESS)
    {
        printf("ERROR: xensiv_bgt60trxx_mtb_init failed\n");
        return false;
    }

	// The sensor will generate an interrupt once the sensor FIFO level is NUM_SAMPLES_PER_FRAME
	result = xensiv_bgt60trxx_mtb_interrupt_init(&bgt60_obj,
			NUM_SAMPLES_PER_FRAME,
			CYBSP_RSPI_IRQ,
			CYHAL_ISR_PRIORITY_DEFAULT,
			radar_isr,
			NULL);
    if (result != CY_RSLT_SUCCESS)
    {
    	printf("ERROR: xensiv_bgt60trxx_mtb_interrupt_init\r\n");
    	return false;
    }

    return true;
}


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

    if (!_init_spi(&spi))
    {
    	printf("init spi error\r\n");
    	return 0;
    }

    if (!_init_bgt60tr13c())
    {
    	printf("init radar error\r\n");
    	return 0;
    }

    printf("all fine so far\r\n");

    // start frames
    if(xensiv_bgt60trxx_start_frame(&bgt60_obj.dev, true) != XENSIV_BGT60TRXX_STATUS_OK)
	{
		printf("Cannot start frame\r\n");
		for(;;){}
	}

    printf("frames are started\r\n");

    uint16_t buffer_raw[NUM_SAMPLES_PER_FRAME];

    for(;;)
    {
    	if (data_available)
    	{
    		data_available = 0;
    		if (xensiv_bgt60trxx_get_fifo_data(&bgt60_obj.dev, buffer_raw, NUM_SAMPLES_PER_FRAME) != XENSIV_BGT60TRXX_STATUS_OK)
    		{
    			printf("xensiv_bgt60trxx_get_fifo_data error\r\n");
    			for(;;){}
    		}
    	}
    }

}

/* [] END OF FILE */

/*
* label_follower.h
*
*  Created on: Sep 11, 2025
*  Company: Rutronik Elektronische Bauelemente GmbH
*  Address: Industriestraße 2, 75228 Ispringen, Germany
*  Author: ROJ030
*
*******************************************************************************
* Copyright 2021-2024, Cypress Semiconductor Corporation (an Infineon company) or
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
*
* Rutronik Elektronische Bauelemente GmbH Disclaimer: The evaluation board
* including the software is for testing purposes only and,
* because it has limited functions and limited resilience, is not suitable
* for permanent use under real conditions. If the evaluation board is
* nevertheless used under real conditions, this is done at one’s responsibility;
* any liability of Rutronik is insofar excluded
*******************************************************************************/

#ifndef LABEL_FOLLOWER_H_
#define LABEL_FOLLOWER_H_

#include <stdint.h>

typedef struct
{
	uint16_t state;
	float threshold_enter;
	float threshold_exit;
} label_follower_t;

/**
 * @brief Init the label follower
 *
 * @param [in] threshold_enter Value for which we consider a state 1
 * @param [in] threshold_exit Value for which we consider the state back to 0
 */
void label_follower_init(label_follower_t* label_follower, float threshold_enter, float threshold_exit);

/**
 * @brief Feed the follower with the predicted value
 *
 * Update the internal state value if needed
 *
 * @retval 0 No change / 1 Change happened
 */
int label_follower_feed(label_follower_t* label_follower, float value);

#endif /* LABEL_FOLLOWER_H_ */

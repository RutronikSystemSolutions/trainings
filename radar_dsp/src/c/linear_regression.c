/*
 * linear_regression.c
 *
 *  Created on: Feb 18, 2025
 *      Author: ROJ030
 */

#include "linear_regression.h"

/**
 * x[] is considered to be an array containing 0, 1, 2, ... len
 */
void linear_regression_compute(float* y, uint16_t len, float* slope, float* intercept)
{
    float xsum = 0;
    float ysum = 0;
    float xysum = 0;
    float xxsum = 0;
    float yysum = 0;
    float count = (float)len;

    // Default
    *slope = 0;
    *intercept = 0;

    if (len == 0) return;

    for (uint16_t i = 0; i < len; i++)
    {
        xsum += (float)i;
        ysum += y[i];
        xysum += (float)i * y[i];
        xxsum += (float)i * (float)i;
        yysum += y[i] * y[i];
    }

    float den = (count * xxsum - xsum * xsum);
    if (den == 0)
    {
    	// Cannot divide by 0
    	return;
    }

    *slope = (count * xysum - xsum * ysum) / den;
    *intercept = (ysum - *slope * xsum) / count;
}

/*
 * radar_processing_internal.h
 *
 *  Created on: Jan 2, 2025
 *      Author: ROJ030
 */

#ifndef RADAR_PROCESSING_GESTURE_PROCESSING_INTERNAL_H_
#define RADAR_PROCESSING_GESTURE_PROCESSING_INTERNAL_H_

#include <stdint.h>

typedef struct
{
	uint8_t antenna_count;
	uint16_t chirps_per_frame;
	uint16_t samples_per_chirp;
	uint32_t sampling_rate;
	uint64_t start_freq;
	uint64_t end_freq;

	uint16_t bin_start;
	uint16_t bin_end;

	float threshold;
} radar_processing_internal_param_t;

#endif /* RADAR_PROCESSING_GESTURE_PROCESSING_INTERNAL_H_ */

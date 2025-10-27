/*
 * radar_processing.h
 *
 *  Created on: Jan 2, 2025
 *      Author: ROJ030
 */

#ifndef RADAR_PROCESSING_GESTURE_PROCESSING_H_
#define RADAR_PROCESSING_GESTURE_PROCESSING_H_

#include <stdint.h>

typedef struct
{
	uint8_t antenna_count;
	uint16_t chirps_per_frame;
	uint16_t samples_per_chirp;
	uint32_t sampling_rate;
	uint64_t start_freq;
	uint64_t end_freq;
} radar_configuration_t;

// Magnitude, Range, Azimuth, Elevation
#define RADAR_PROCESSING_OUTPUT_DIMENSION 4

typedef struct
{
	float amplitude;
	float range;
	float azimuth;
	float elevation;
} radar_processing_out_t;

int radar_processing_init(radar_configuration_t radar_configuration);

void radar_processing_feed(uint16_t * frame_samples, radar_processing_out_t* result);

#endif /* RADAR_PROCESSING_GESTURE_PROCESSING_H_ */

/*
 * radar_processing.c
 *
 *  Created on: Jan 2, 2025
 *      Author: ROJ030
 */

#include "radar_processing.h"
#include "range_fft.h"
#include "doppler_fft.h"
#include "radar_processing_internal.h"

#include <stdlib.h>

#undef DEBUG_RANGE_AZIMUTH
#define DEBUG_DATASET

#if defined(DEBUG_RANGE_AZIMUTH) || defined(DEBUG_DATASET)
#include <stdio.h>
#endif

/**
 * @var adc_samples
 * Store the converted ADC samples.
 * This buffer is used as source for the range FFT (since the raw frame_samples contains interleaved samples).
 */
static float* adc_samples = NULL;

/**
 * @var window
 * Window used to be applied on the time signal before computing real FFT
 */
static float* window = NULL;

/**
 * @var doppler_window
 * Window used to be applied on the range FFT signal before computing doppler FFT
 */
static float* doppler_window = NULL;

/**
 * @var range
 * Store the output of the range computation
 * Allocated once at start
 * Size is antenna count * chirps per frame * (samples per chirp / 2) * sizeof(cfloat)
 * Why samples per chirp / 2 and not (samples per chirp) / 2 + 1 -> because of the implementation of the FFT
 */
static cfloat32_t* range = NULL;

/**
 * @var doppler_out
 * Store the result of the doppler FFT for one bin
 * doppler_out is the output of the FFT of complex values
 */
static cfloat32_t* doppler_out = NULL;

static radar_processing_internal_param_t internal_params;


int radar_processing_init(radar_configuration_t radar_configuration)
{
	// Save
	internal_params.antenna_count = radar_configuration.antenna_count;
	internal_params.chirps_per_frame = radar_configuration.chirps_per_frame;
	internal_params.samples_per_chirp = radar_configuration.samples_per_chirp;
	internal_params.sampling_rate = radar_configuration.sampling_rate;
	internal_params.start_freq = radar_configuration.start_freq;
	internal_params.end_freq = radar_configuration.end_freq;

	// internal_params.threshold = 0.1;
	internal_params.threshold = 0.05;

	// Compute bin_start and bin_end
	// Total range
	const uint16_t fft_len = internal_params.samples_per_chirp / 2;
	internal_params.bin_start = 0;
	internal_params.bin_end = fft_len;

	// Allocate
	adc_samples = (float*) malloc(radar_configuration.samples_per_chirp * sizeof(float));
	if (adc_samples == NULL) return -5;

	range = (cfloat32_t*) malloc(radar_configuration.antenna_count * radar_configuration.chirps_per_frame * (radar_configuration.samples_per_chirp / 2) * sizeof(cfloat32_t));
	if (range == NULL) return -6;

	doppler_out = (cfloat32_t*) malloc(radar_configuration.chirps_per_frame * sizeof(cfloat32_t));
	if (range == NULL) return -7;

	// Generate window
	window = (float*) malloc(radar_configuration.samples_per_chirp * sizeof(float));
	if (window == NULL) return -8;
	ifx_window_blackmanharris_f32(window, radar_configuration.samples_per_chirp);

	// Generate doppler window (applied before computing doppler FFT)
	doppler_window = (float*) malloc(radar_configuration.chirps_per_frame * sizeof(float));
	if (doppler_window == NULL) return -9;
	ifx_window_blackmanharris_f32(doppler_window, radar_configuration.chirps_per_frame);

	return 0;
}

static float get_magnitude(cfloat32_t complex_value)
{
	float32_t* value = (float32_t*)&complex_value;
	float32_t real = value[0];
	float32_t imag = value[1];
	float32_t magnitude = 0;

	arm_sqrt_f32((real * real) + (imag * imag), &magnitude);

	return magnitude;
}

static float get_phase(cfloat32_t complex_value)
{
	float32_t* value = (float32_t*)&complex_value;
	float32_t real = value[0];
	float32_t imag = value[1];

	return atan2f(imag, real);
}

static void get_max_magnitude_phase_velocity(cfloat32_t* array, uint16_t len, float* mag_out, float* phase_out, uint16_t* velocity_out)
{
	float max = 0;
	uint16_t max_index = 0;
	for(uint16_t i = 0; i < len; ++i)
	{
		float mag = get_magnitude(array[i]);
		if (mag > max)
		{
			max = mag;
			max_index = i;
		}
	}

	*mag_out = max;
	*phase_out = get_phase(array[max_index]);
	*velocity_out = max_index;
}

static float f32abs(float a)
{
	if (a > 0) return a;
	return -a;
}

static float get_angle_diff(float a1, float a2)
{
    float sign = -1;
    if (a1 > a2)
        sign = 1;

    float angle = a1 - a2;
    float k = -sign * M_PI * 2;

    if (f32abs(k + angle) < f32abs(angle))
    	return (k + angle);

    return angle;
}

void radar_processing_feed(uint16_t * frame_samples, radar_processing_out_t* result)
{
	const uint16_t fft_len = internal_params.samples_per_chirp / 2;

	// Compute range FFT of the frame. For each chirp compute a FFT -> output inside "range"
	// only compute for RX1 and RX3 (since we only consider the azimuth so far)
	range_fft_do(frame_samples,
			range,
			adc_samples,
			true,				// remove mean
			window,				// window (Blackman Harris)
			internal_params.antenna_count,
			// 5,					// antenna mask, 0b101 -> RX1 and RX3 (do not compute RX2)
			7, // antenna mask, 0b111 -> RX1, RX2 and RX3
			internal_params.samples_per_chirp,
			internal_params.chirps_per_frame);

	// Compute Doppler FFT for each bin (only for antenna 0 to save time)
	// Bin index from [0] to [(samples per chirp / 2) - 1]
	// and extract maximum
	float maximum_doppler = 0;
	uint16_t max_bin_idx = 0;
	float phase_rx1 = 0;
	uint16_t velocity_rx1 = 0;

	for(uint16_t bin_idx = internal_params.bin_start; bin_idx < internal_params.bin_end; ++bin_idx)
	{
		doppler_fft_bin_do(range,
				doppler_out,		// Doppler FFT output (size is chirps_per_frame)
				true,				// Remove mean (0 m/s speed)
				doppler_window,		// Window
				bin_idx,			// Bin index
				0, 					// Antenna index -> RX1
				internal_params.chirps_per_frame,
				fft_len);

		// Get maximum amplitude (and phase for it)
		float max_magnitude = 0;
		float max_phase = 0;
		uint16_t max_velocity = 0;

		get_max_magnitude_phase_velocity(doppler_out, internal_params.chirps_per_frame, &max_magnitude, &max_phase, &max_velocity);
		if (max_magnitude > maximum_doppler)
		{
			maximum_doppler = max_magnitude;
			max_bin_idx = bin_idx;
			phase_rx1 = max_phase;
			velocity_rx1 = max_velocity;
		}
	}

	result->amplitude = maximum_doppler;

	if (maximum_doppler > internal_params.threshold)
	{
		// Need to compute doppler FFT (but only for range idx = max_bin_idx
		// we then store the magnitude, the range and the angle difference
		// then compute the phase difference and store it

		doppler_fft_bin_do(range,
				doppler_out,		// Doppler FFT output (size is chirps_per_frame)
				true,				// Remove mean (0 m/s speed)
				doppler_window,		// Window
				max_bin_idx,		// Bin index -> Max of RX1
				2, 					// Antenna index -> RX3
				internal_params.chirps_per_frame,
				fft_len);

		float phase_rx3 = get_phase(doppler_out[velocity_rx1]);

		doppler_fft_bin_do(range,
				doppler_out,		// Doppler FFT output (size is chirps_per_frame)
				true,				// Remove mean (0 m/s speed)
				doppler_window,		// Window
				max_bin_idx,		// Bin index -> Max of RX1
				1, 					// Antenna index -> RX2
				internal_params.chirps_per_frame,
				fft_len);

		float phase_rx2 = get_phase(doppler_out[velocity_rx1]);

		float azimuth = get_angle_diff(phase_rx1, phase_rx3);
		float elevation = get_angle_diff(phase_rx2, phase_rx3);

#ifdef DEBUG_RANGE_AZIMUTH
		printf("%d;%f;\r\n", max_bin_idx, delta);
#endif

		// max_bin_idx: range
		// delta: angle of arrival
		result->azimuth = azimuth;
		result->range = max_bin_idx;
		result->elevation = elevation;
	}
	else
	{
		// Below threshold
		result->azimuth = 0;
		result->range = 0;
		result->elevation = 0;
	}
}



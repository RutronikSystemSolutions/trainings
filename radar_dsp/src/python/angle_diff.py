import matplotlib.pyplot as plt
import numpy as np
import json
from scipy import signal
import math

# Get the difference between 2 angles
# Always between -pi and pi
def get_diff(a1, a2):
    sign = -1
    if (a1 > a2):
        sign = 1

    angle = a1 - a2
    k = -sign * math.pi * 2
    if (np.abs(k + angle) < np.abs(angle)):
        return k + angle

    return angle

# Compute the Doppler FFT for an antenna
def get_doppler_fft(radar_data, antenna_index, frame_index):
    # Compute range FFT
    range_fft_array = []
    for chirp_index in range(chirps_per_frame):
        time_signal = radar_data[frame_index][antenna_index][chirp_index]

        # Scale between 0 and 1 - remove DC
        time_signal = time_signal / 4095
        time_signal = time_signal - (np.sum(time_signal) / len(time_signal))

        # Apply window
        window = signal.windows.blackmanharris(len(time_signal))
        time_signal = time_signal * window

        # Compute FFT
        fft_output = np.fft.rfft(time_signal)

        range_fft_array.append(fft_output)

    # Compute doppler FFT (1 FFT per bin)
    doppler_fft = []
    for bin_index in range(fft_len):
        bin_array = []
        for chirp_index in range(chirps_per_frame):
            bin_array.append(range_fft_array[chirp_index][bin_index])

        # Remove DC
        bin_array = bin_array - (np.sum(bin_array) / len(bin_array))

        # Apply window
        window = signal.windows.blackmanharris(chirps_per_frame)
        bin_array = bin_array * window    

        # Compute FFT
        fft_output = np.fft.fft(bin_array)
        fft_output = np.fft.fftshift(fft_output)

        doppler_fft.append(fft_output)

    return doppler_fft


radar_data = np.load("data\\sample_1\\RadarIfxAvian_00\\radar.npy")

configuration_file = open("data\\sample_1\\RadarIfxAvian_00\\config.json")
radar_configuration = json.load(configuration_file)
configuration_file.close()

sampling_frequency = int(
    radar_configuration
    ['device_config']['fmcw_single_shape']['sample_rate_Hz'])

start_frequency = int(
    radar_configuration
    ['device_config']['fmcw_single_shape']['start_frequency_Hz'])

end_frequency = int(
    radar_configuration
    ['device_config']['fmcw_single_shape']['end_frequency_Hz'])

samples_per_chirp = int(
    radar_configuration
    ['device_config']['fmcw_single_shape']['num_samples_per_chirp'])

chirps_per_frame = int(
    radar_configuration
    ['device_config']['fmcw_single_shape']['num_chirps_per_frame'])

celerity = 299792458

fft_len = int((samples_per_chirp / 2) + 1)

slope = (end_frequency - start_frequency) / (samples_per_chirp * (1. / sampling_frequency))
maximum_range = ((sampling_frequency / 2.)  * celerity) / (2. * slope)
print("Maximum range: " + str(maximum_range))
print("FFT len: " + str(fft_len))

lambdaval = celerity / ((start_frequency + end_frequency)/2.)

magnitude_over_time = []
angle_over_time = []

for frame_index in range(len(radar_data)):
    doppler_fft_rx1 = get_doppler_fft(radar_data, 0, frame_index)
    # Remark, use of doppler_fft:
    # doppler_fft[bin index][velocity index]
    # Get the maximum magnitude
    max_magnitude = np.abs(doppler_fft_rx1[0][0])
    max_i = 0
    max_j = 0
    for i in range(len(doppler_fft_rx1)):
        for j in range(len(doppler_fft_rx1[i])):
            if (np.abs(doppler_fft_rx1[i][j]) > max_magnitude):
                max_magnitude = np.abs(doppler_fft_rx1[i][j])
                max_i = i
                max_j = j

    if (max_magnitude > 0.5):
        doppler_fft_rx3 = get_doppler_fft(radar_data, 2, frame_index)
        phaseRx1 = np.angle(doppler_fft_rx1[max_i][max_j])
        phaseRx3 = np.angle(doppler_fft_rx3[max_i][max_j])
        phaseDiff = get_diff(phaseRx1, phaseRx3)
        alpha = np.sinh((lambdaval * phaseDiff) / (2 * np.pi * 2.5E-3))
        angle_over_time.append(alpha)

    else:
        angle_over_time.append(0.)

    magnitude_over_time.append(max_magnitude)

plt.figure()
plt.plot(magnitude_over_time)
plt.grid(True)
plt.xlabel("Frame index")

plt.figure()
plt.plot(angle_over_time, 'o')
plt.grid(True)
plt.xlabel("Frame index")
plt.show()
exit(0)

plt.figure()
plt.imshow(np.abs(doppler_fft_rx1), aspect='auto', interpolation='nearest',
           origin='lower')
plt.xlabel("Velocity")
plt.ylabel("Range")
plt.colorbar()

plt.figure()
plt.imshow(np.abs(doppler_fft_rx3), aspect='auto', interpolation='nearest',
           origin='lower')
plt.xlabel("Velocity")
plt.ylabel("Range")
plt.colorbar()

plt.show()

exit(0)
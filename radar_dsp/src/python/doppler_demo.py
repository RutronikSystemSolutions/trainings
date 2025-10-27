import matplotlib.pyplot as plt
import numpy as np
import json
from scipy import signal

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

#frame_index = 33

magnitude_over_time = []

for frame_index in range(len(radar_data)):
    # Compute range FFT
    range_fft_array = []
    for chirp_index in range(chirps_per_frame):
        time_signal = radar_data[frame_index][0][chirp_index]

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


    # Remark, use of doppler_fft:
    # doppler_fft[bin index][velocity index]
    # Get the maximum magnitude
    max_magnitude = np.abs(doppler_fft[0][0])
    for i in range(len(doppler_fft)):
        for j in range(len(doppler_fft[i])):
            if (np.abs(doppler_fft[i][j]) > max_magnitude):
                max_magnitude = np.abs(doppler_fft[i][j])

    magnitude_over_time.append(max_magnitude)

plt.plot(magnitude_over_time)
plt.grid(True)
plt.xlabel("Frame index")



# Remark concerning range_fft_array:
# range_fft_array[chirp index][bin index]

# Plot phase over time
selected_bin = 5
phase_over_time = []
for chirp_index in range(chirps_per_frame):
    phase_over_time.append(np.angle(range_fft_array[chirp_index][selected_bin]))

plt.figure()
plt.plot(phase_over_time)
plt.ylim([-np.pi, np.pi])
plt.xlabel("Chirp index")
plt.ylabel("Phase (rad)")
plt.grid(True)


plt.figure()
plt_legend = []
for bin_index in range(fft_len):
    plt.plot(np.abs(doppler_fft[bin_index]))
    plt_legend.append("Bin " + str(bin_index))

plt.xlabel("Velocity")
plt.ylabel("Amplitude")
plt.legend(plt_legend)
plt.grid(True)

plt.figure()
plt.imshow(np.abs(doppler_fft), aspect='auto', interpolation='nearest',
           origin='lower')
plt.xlabel("Velocity")
plt.ylabel("Range")
plt.colorbar()
plt.show()

exit(0)
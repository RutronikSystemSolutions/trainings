import matplotlib.pyplot as plt
import numpy as np
import json
from scipy import signal

print('hi')


radar_data = np.load("data\\sample_1\\RadarIfxAvian_00\\radar.npy")

configuration_file = open("data\\sample_1\\RadarIfxAvian_00\\config.json")
radar_configuration = json.load(configuration_file)
configuration_file.close()

'''
# Part 1
# radar_data[frame_index][antenna_index][chirp_index]
plt.figure()
plt.plot(radar_data[0][0][0])
plt.plot(radar_data[0][1][0])
plt.plot(radar_data[0][2][0])
plt.legend(["Ant. 0", "Ant. 1", "Ant. 2"])
plt.grid(True)
plt.show()

# radar_data[frame_index][antenna_index][chirp_index]
plt.figure()
plt.plot(radar_data[0][0][0])
plt.plot(radar_data[0][0][32])
plt.plot(radar_data[0][0][63])
plt.legend(["Chirp 0", "Chirp 32", "Chirp 63"])
plt.grid(True)
plt.show()

exit(0)
'''

# Part 2
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

celerity = 299792458

slope = (end_frequency - start_frequency) / (samples_per_chirp * (1. / sampling_frequency))
maximum_range = ((sampling_frequency / 2.)  * celerity) / (2. * slope)
print("Maximum range: " + str(maximum_range))

'''
time_signal = radar_data[0][0][0]

# Scale between 0 and 1 - remove DC
time_signal = time_signal / 4095
time_signal = time_signal - (np.sum(time_signal) / len(time_signal))

# Apply window
window = signal.windows.blackmanharris(len(time_signal))
source_signal = time_signal
time_signal = time_signal * window

# Compute FFT
fft_output = np.fft.rfft(time_signal)

# Normalize
fft_output_scaled = np.abs(fft_output) * 2 / np.sum(window)
fft_output_dbfs = 20 * np.log10(fft_output_scaled / 1)

# Compute frequency and meter axis
freq_axis = np.fft.rfftfreq(len(time_signal), d=1./sampling_frequency)
meter_axis = np.linspace(0, maximum_range, len(fft_output))

plt.figure()
plt.plot(meter_axis, fft_output_dbfs)
plt.grid(True)
plt.show()
'''

range_fft_array = []
for frame_index in range(len(radar_data)):
    time_signal = radar_data[frame_index][0][0]

    # Scale between 0 and 1 - remove DC
    time_signal = time_signal / 4095
    time_signal = time_signal - (np.sum(time_signal) / len(time_signal))

    # Apply window
    window = signal.windows.blackmanharris(len(time_signal))
    source_signal = time_signal
    time_signal = time_signal * window

    # Compute FFT
    fft_output = np.fft.rfft(time_signal)

    range_fft_array.append(fft_output)

# Remove first from all other
for frame_index in range(len(radar_data)):
    if (frame_index == 0):
        continue

    for sample_index in range(len(range_fft_array[0])):
        range_fft_array[frame_index][sample_index] = range_fft_array[frame_index][sample_index] -  range_fft_array[0][sample_index]

for sample_index in range(len(range_fft_array[0])):
    range_fft_array[0][sample_index] = 0


plt.imshow(np.abs(range_fft_array), aspect='auto', interpolation='nearest',
                        origin='lower')
plt.xlabel("Range")
plt.ylabel("Time")
plt.show()

###
exit(0)

sample_count = int(
    radar_configuration
    ['device_config']['fmcw_single_shape']['num_samples_per_chirp'])

print("Sample count: " + str(sample_count))

'''
values = [1, 2, 4, 10, 12, 23, 14, 5, 7, 18]
plt.plot(values)
plt.grid(True)
plt.show()
'''
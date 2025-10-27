import numpy as np
import matplotlib.pyplot as plt
from scipy import signal

print('FFT demo')

# Sinus wave -> f(t) = A * sin (w.t)
# w = 2 . Pi * frequency
duration_sec = 0.5
frequency = 10
sampling_frequency = 500
amplitude = 1

x = np.arange(0, duration_sec, 1./sampling_frequency)

y = amplitude * np.sin(2 * np.pi * frequency * x)
# y = y + amplitude * np.sin(2 * np.pi * 20 * x)

# Apply window
window = signal.windows.blackmanharris(len(y))
source_signal = y
y = y * window

# Compute FFT
fft_output = np.fft.rfft(y)
freq_axis = np.fft.rfftfreq(len(y), d=1./sampling_frequency)

freq_resolution = (sampling_frequency / 2.) / (len(fft_output) - 1)
print("Frequency resolution: " + str(freq_resolution))
print("Samples count: " + str(len(y)))
print("FFT size: " + str(len(fft_output)))

fft_output_scaled = np.abs(fft_output) * 2 / np.sum(window)
fft_output_dbfs = 20 * np.log10(fft_output_scaled / 1)

plt.figure()
plt.plot(x, y)
plt.plot(x, source_signal)
plt.plot(x, window)
plt.legend(["Windowed", "Raw", "Window"])
plt.grid(True)

plt.figure()
# plt.plot(freq_axis, np.abs(fft_output) / (len(y)/2))
plt.plot(freq_axis, fft_output_dbfs)
plt.grid(True)
plt.show()
exit(0)


plt.show()

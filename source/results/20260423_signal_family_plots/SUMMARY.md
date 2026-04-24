# Signal Family Reference Plots

These plots are generated from the implemented firmware signal definitions:
- `clean`: `2*sin(2*pi*3*t) + 4*sin(2*pi*5*t)`
- `noise`: clean signal + Gaussian noise with `sigma=0.2`
- `spikes`: noise signal + signed uniform spike injection in `+-U(5, 15)` with `p=2%`

Sampling assumptions for the reference plots: `FFT_N=256`, baseline `fs=100 Hz`, `6` synthetic FFT windows per signal.

## clean

- Dominant mean frequency: `5.078 Hz`
- Adaptive fs mean/min/max: `10.156 / 10.156 / 10.156 Hz`
- Main spectrum peaks: `5.08 Hz`, `4.69 Hz`, `3.12 Hz`
- Plots: `clean_waveform.svg`, `clean_spectrum.svg`, `clean_adaptive_fs.svg`

## noise

- Dominant mean frequency: `5.078 Hz`
- Adaptive fs mean/min/max: `10.156 / 10.156 / 10.156 Hz`
- Main spectrum peaks: `5.08 Hz`, `4.69 Hz`, `3.12 Hz`
- Plots: `noise_waveform.svg`, `noise_spectrum.svg`, `noise_adaptive_fs.svg`

## spikes

- Dominant mean frequency: `5.078 Hz`
- Adaptive fs mean/min/max: `10.156 / 10.156 / 10.156 Hz`
- Main spectrum peaks: `5.08 Hz`, `4.69 Hz`, `3.12 Hz`
- Plots: `spikes_waveform.svg`, `spikes_spectrum.svg`, `spikes_adaptive_fs.svg`


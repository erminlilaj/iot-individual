# Signal B

- Formula: `4*sin(2*pi*3*t) + 2*sin(2*pi*9*t)`
- Expected highest frequency: `9.00 Hz`
- Observed dominant frequency: `3.01 Hz`
- Adaptive sampling rate: `25.00 Hz`
- 5 s window sample count at adaptive rate: `125`
- Reference waveform window: seq `1`, `fs=100.000 Hz`, `n=256`
- Main spectrum peaks in the plotted window: `3.12 Hz`, `2.73 Hz`, `8.98 Hz`
- Interpretation: the 9 Hz tone exists, but the 3 Hz tone is stronger, so the controller follows the dominant peak and the current 8x policy targets about 25 Hz.

Waveform and spectrum plots are generated from the implemented signal formula. The dominant frequency values are from the captured DUT/reference sessions, and the adaptive sampling rate is recomputed using the current firmware policy.

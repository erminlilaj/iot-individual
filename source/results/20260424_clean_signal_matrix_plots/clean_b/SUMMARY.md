# Signal B

- Formula: `4*sin(2*pi*3*t) + 2*sin(2*pi*9*t)`
- Expected highest frequency: `9.00 Hz`
- Observed dominant frequency: `3.01 Hz`
- Adaptive sampling rate: `10.00 Hz`
- Reference waveform window: seq `1`, `fs=100.000 Hz`, `n=256`
- Main spectrum peaks in the plotted window: `3.12 Hz`, `2.73 Hz`, `8.98 Hz`
- Interpretation: the 9 Hz tone exists, but the 3 Hz tone is stronger, so the controller follows the dominant peak instead of the highest-frequency component.

Waveform and spectrum plots are generated from the implemented signal formula. The observed dominant frequency and adaptive sampling rate come from the measured DUT session listed above.

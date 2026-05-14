# Signal A

- Formula: `2*sin(2*pi*3*t) + 4*sin(2*pi*5*t)`
- Expected highest frequency: `5.00 Hz`
- Observed dominant frequency: `5.02 Hz`
- Adaptive sampling rate: `40.00 Hz`
- 5 s window sample count at adaptive rate: `200`
- Reference waveform window: seq `1`, `fs=100.000 Hz`, `n=256`
- Main spectrum peaks in the plotted window: `5.08 Hz`, `4.69 Hz`, `3.12 Hz`
- Interpretation: reference case where the dominant and highest tone are the same, so the current 8x policy adapts to 40 Hz.

Waveform and spectrum plots are generated from the implemented signal formula. The dominant frequency values are from the captured DUT/reference sessions, and the adaptive sampling rate is recomputed using the current firmware policy.

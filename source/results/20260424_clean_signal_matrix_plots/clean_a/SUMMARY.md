# Signal A

- Formula: `2*sin(2*pi*3*t) + 4*sin(2*pi*5*t)`
- Expected highest frequency: `5.00 Hz`
- Observed dominant frequency: `5.00 Hz`
- Adaptive sampling rate: `10.00 Hz`
- Reference waveform window: seq `1`, `fs=100.000 Hz`, `n=256`
- Main spectrum peaks in the plotted window: `5.08 Hz`, `4.69 Hz`, `3.12 Hz`
- Interpretation: baseline case where the dominant and highest tone are the same, so adaptation matches the Nyquist expectation cleanly.

Waveform and spectrum plots are generated from the implemented signal formula. The observed dominant frequency and adaptive sampling rate come from the measured DUT session listed above.

# Signal C

- Formula: `2*sin(2*pi*2*t) + 3*sin(2*pi*5*t) + 1.5*sin(2*pi*7*t)`
- Expected highest frequency: `7.00 Hz`
- Observed dominant frequency: `5.03 Hz`
- Adaptive sampling rate: `10.10 Hz`
- Reference waveform window: seq `1`, `fs=100.000 Hz`, `n=256`
- Main spectrum peaks in the plotted window: `5.08 Hz`, `1.95 Hz`, `4.69 Hz`
- Interpretation: with three tones present, the 5 Hz component still dominates, so the adaptive rate stays close to 10 Hz instead of rising toward 14 Hz.

Waveform and spectrum plots are generated from the implemented signal formula. The observed dominant frequency and adaptive sampling rate come from the measured DUT session listed above.

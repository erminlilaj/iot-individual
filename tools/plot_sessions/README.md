# Plot Sessions

This folder contains raw timestamped capture sessions produced by `tools/plot_capture.py`.

Read the sessions in this order:

1. `20260422_120509_clean_dut_no_ina219_60s_v2`
2. `20260422_115402_clean_dut_no_ina219_60s`
3. older folders only if you need debugging history

## Session Guide

### `20260422_120509_clean_dut_no_ina219_60s_v2`

Canonical validated session used for the current report figures and curated export under `source/results/`.

- clean-mode DUT run
- `60 s` capture
- `INA219` disabled
- `5` MQTT aggregate windows matched at the edge listener
- `5` LoRa uplinks captured
- `3` FFT updates at `5.00 Hz -> 10.0 Hz`
- no panic or reboot signatures in `serial.log`

### `20260422_115402_clean_dut_no_ina219_60s`

Earlier clean `60 s` session that already produced the full figure set, but it was superseded by the `v2` rerun with the dedicated LoRa figure and final category checks.

### `20260422_115319_clean_dut_no_ina219_full`

Intermediate no-INA219 run kept mainly for debugging and comparison.

### `20260422_115028_clean_dut_no_ina219`

Earlier shortened no-INA219 run kept as historical evidence during the crash-fix pass.

### `20260422_114242_clean_dut`

Useful mainly as a contrast session because LoRa join was still failing there.

## Per-Session File Meaning

Inside each session folder:

- `serial.log`: full timestamped DUT serial stream
- `metadata.json`: capture settings
- `fft.csv`: parsed FFT updates
- `agg.csv`: parsed 5 s aggregation windows
- `mqtt_send.csv`: MQTT publishes reported by the DUT
- `mqtt_rx.csv`: MQTT messages observed by the passive edge subscriber
- `latency.csv`: parsed MQTT RTT samples
- `lora.csv`: parsed LoRa uplinks when present
- `plot_samples.csv`: raw sample window used to generate waveform and spectrum plots
- `plots/`: generated report figures when `tools/generate_session_plots.py` has been run

## Recommended Reading Path

If you are checking one session quickly:

1. open `metadata.json`
2. open `serial.log`
3. inspect `agg.csv`, `fft.csv`, `latency.csv`, and `lora.csv`
4. if it is the final validated session, compare it with `source/results/README.md`

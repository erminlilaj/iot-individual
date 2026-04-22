# DUT Results Summary

Session source:

`tools/plot_sessions/20260422_120509_clean_dut_no_ina219_60s_v2/`

This bundle contains the latest validated `60 s` DUT capture with `INA219` disabled.

## Contents

- `results_agg.csv`: quick-access aggregation export
- `results_fft.csv`: quick-access FFT export
- `results_latency.csv`: quick-access MQTT latency export
- `results_lora.csv`: quick-access LoRa latency export
- `01_waveform_snapshot.png` to `06_lora_latency.png`: quick-access final figures
- `data/agg.csv`: 5 s aggregation windows
- `data/fft.csv`: FFT dominant frequency and adaptive sampling updates
- `data/latency.csv`: MQTT RTT measurements
- `data/lora.csv`: LoRa uplink events and end-to-end latency
- `data/mqtt_rx.csv`: MQTT values received at the edge listener
- `data/mqtt_send.csv`: MQTT values published by the DUT
- `data/plot_samples.csv`: raw FFT-window samples used for waveform and spectrum plots
- `plots/01_waveform_snapshot.png`
- `plots/02_fft_spectrum.png`
- `plots/03_adaptive_fs.png`
- `plots/04_aggregate_mqtt_path.png`
- `plots/05_mqtt_latency_distribution.png`
- `plots/06_lora_latency.png`
- `logs/serial.log`: raw DUT serial output
- `pics/`: supporting screenshots from the demo/evidence set

## Key Results

- Waveform status: normal
- FFT dominant frequency: `5.00 Hz` normal
- Adaptive sampling rate: `10.00 Hz` normal
- Aggregation window: `n=50` normal
- MQTT delivery: `5/5` matched at the edge listener
- MQTT RTT: mean `630.6 ms`, max `891.0 ms`, normal
- LoRa uplinks: `5` captured, mean e2e `11829.2 ms`, range `1589-24863 ms`, normal for LoRaWAN

## Notes

- This run showed no panic or reboot signatures in the final serial log.
- The waveform/spectrum snapshot was captured after adaptation at `10 Hz`, so the plots emphasize the dominant `~5 Hz` component.
- The canonical detailed ledger remains in `docs/plot_data_records.md`.

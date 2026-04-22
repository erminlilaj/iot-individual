# Plot Data Records

Session folder: `tools/plot_sessions/20260422_120509_clean_dut_no_ina219_60s_v2`

This file records the exact data sources used to generate the report/defense plots for the latest captured DUT session.

## Figure 1 — DUT waveform snapshot

- Output: [01_waveform_snapshot.png](01_waveform_snapshot.png)
- Sources: `plot_samples.csv`, `metadata.json`
- Samples used: `256`
- Sampling rate: `10.000 Hz`
- Value range: `-5.6731` to `+5.6731`

## Figure 2 — DUT FFT magnitude spectrum

- Output: [02_fft_spectrum.png](02_fft_spectrum.png)
- Sources: `plot_samples.csv`
- Samples used: `256`
- Sampling rate: `10.000 Hz`
- Strongest peaks: `4.96 Hz`, `5.00 Hz`

## Figure 3 — Adaptive sampling rate over time

- Output: [03_adaptive_fs.png](03_adaptive_fs.png)
- Sources: `fft.csv`
- FFT updates: `3`
- Mean dominant frequency: `5.000 Hz`
- Mean adaptive sampling rate: `10.000 Hz`
- Adaptive sampling range: `10.000 Hz` to `10.000 Hz`

## Figure 4 — Aggregate values across the MQTT path

- Output: [04_aggregate_mqtt_path.png](04_aggregate_mqtt_path.png)
- Sources: `agg.csv`, `mqtt_send.csv`, `mqtt_rx.csv`
- Aggregate windows logged: `5`
- MQTT sent messages: `5`
- MQTT received messages: `5`
- Matched sent/received messages plotted: `5`

## Figure 5 — MQTT latency distribution

- Output: [05_mqtt_latency_distribution.png](05_mqtt_latency_distribution.png)
- Sources: `latency.csv`
- RTT samples: `5`
- Mean RTT: `630.6 ms`
- Median RTT: `513.0 ms`
- p95 RTT: `875.4 ms`
- Max RTT: `891.0 ms`

## Figure 6 — LoRa uplink latency

- Output: [06_lora_latency.png](06_lora_latency.png)
- Sources: `lora.csv`
- LoRa uplinks captured: `5`
- Mean LoRa e2e latency: `11829.2 ms`
- Min LoRa e2e latency: `1589.0 ms`
- Max LoRa e2e latency: `24863.0 ms`

## Assessment By Data Category

These status labels are inferred from the project's expected clean-mode behavior.

- `waveform`: **normal** — peak amplitude about 5.67, expected around the clean signal range
- `fft_dominant_frequency`: **normal** — mean dominant frequency 5.00 Hz, expected about 5 Hz
- `adaptive_sampling_rate`: **normal** — adaptive fs mean 10.00 Hz, expected about 10 Hz after Nyquist adaptation
- `aggregation_window`: **normal** — sample counts 50-50 and mean magnitude avg 0.0005
- `mqtt_delivery`: **normal** — matched 5/5 aggregate publishes at the edge listener
- `mqtt_latency`: **normal** — RTT mean 630.6 ms, max 891.0 ms
- `lora_uplink`: **normal** — 5 uplinks captured, latency range 1589-24863 ms which is high but still realistic for LoRaWAN

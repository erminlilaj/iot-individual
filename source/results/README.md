# Results

This folder contains curated submission-style exports of validated DUT runs.

Start with:

- [20260422_clean_dut_no_ina219_60s_v2/SUMMARY.md](20260422_clean_dut_no_ina219_60s_v2/SUMMARY.md)
- [20260422_clean_dut_no_ina219_60s_v2/PLOT_DATA_RECORDS.md](20260422_clean_dut_no_ina219_60s_v2/PLOT_DATA_RECORDS.md)

The raw upstream capture session is indexed separately in:

- [tools/plot_sessions/README.md](../../tools/plot_sessions/README.md)

## Available Bundle

### `20260422_clean_dut_no_ina219_60s_v2`

Current canonical export used for the report-style figures and final evidence bundle.

Quick summary:

- clean-mode DUT run
- `60 s` capture
- `5` MQTT aggregate windows matched at the edge listener
- `5` LoRa uplinks captured
- `3` FFT updates at `5.00 Hz -> 10.0 Hz`
- no panic or reboot signatures in the exported serial log

Bundle layout:

- `SUMMARY.md`: one-page human-readable summary
- `PLOT_DATA_RECORDS.md`: figure-by-figure data provenance
- top-level `results_*.csv`: quick-access result tables
- top-level `0X_*.png`: quick-access final report figures
- `data/`: full parsed CSV outputs
- `plots/`: generated figure copies
- `logs/`: raw serial log from the DUT
- `pics/`: supporting screenshots copied from the project evidence set
- `metadata.json`: capture settings copied from the raw session

Exported from:

`tools/plot_sessions/20260422_120509_clean_dut_no_ina219_60s_v2/`

#### Generated Figures

**Figure 1**

![Waveform snapshot](20260422_clean_dut_no_ina219_60s_v2/01_waveform_snapshot.png)

Short description: raw DUT waveform snapshot from one captured FFT window after adaptation stabilized at `10 Hz`.

References:
- Figure file: [01_waveform_snapshot.png](20260422_clean_dut_no_ina219_60s_v2/01_waveform_snapshot.png)
- Sample data: [plot_samples.csv](20260422_clean_dut_no_ina219_60s_v2/data/plot_samples.csv)
- Serial log: [serial.log](20260422_clean_dut_no_ina219_60s_v2/logs/serial.log)

**Figure 2**

![FFT spectrum](20260422_clean_dut_no_ina219_60s_v2/02_fft_spectrum.png)

Short description: FFT magnitude spectrum computed from the same captured window, showing the dominant component around `5 Hz`.

References:
- Figure file: [02_fft_spectrum.png](20260422_clean_dut_no_ina219_60s_v2/02_fft_spectrum.png)
- Sample data: [plot_samples.csv](20260422_clean_dut_no_ina219_60s_v2/data/plot_samples.csv)
- Serial log: [serial.log](20260422_clean_dut_no_ina219_60s_v2/logs/serial.log)

**Figure 3**

![Adaptive sampling](20260422_clean_dut_no_ina219_60s_v2/03_adaptive_fs.png)

Short description: adaptive sampling history across the captured run, confirming that FFT repeatedly drove the system to `10.0 Hz`.

References:
- Figure file: [03_adaptive_fs.png](20260422_clean_dut_no_ina219_60s_v2/03_adaptive_fs.png)
- FFT data: [results_fft.csv](20260422_clean_dut_no_ina219_60s_v2/results_fft.csv)
- Serial log: [serial.log](20260422_clean_dut_no_ina219_60s_v2/logs/serial.log)

**Figure 4**

![Aggregate values across MQTT](20260422_clean_dut_no_ina219_60s_v2/04_aggregate_mqtt_path.png)

Short description: comparison of the 5-second aggregate values produced on the DUT and received on the MQTT edge path.

References:
- Figure file: [04_aggregate_mqtt_path.png](20260422_clean_dut_no_ina219_60s_v2/04_aggregate_mqtt_path.png)
- Aggregate data: [results_agg.csv](20260422_clean_dut_no_ina219_60s_v2/results_agg.csv)
- MQTT send data: [mqtt_send.csv](20260422_clean_dut_no_ina219_60s_v2/data/mqtt_send.csv)
- MQTT receive data: [mqtt_rx.csv](20260422_clean_dut_no_ina219_60s_v2/data/mqtt_rx.csv)
- Serial log: [serial.log](20260422_clean_dut_no_ina219_60s_v2/logs/serial.log)

**Figure 5**

![MQTT latency distribution](20260422_clean_dut_no_ina219_60s_v2/05_mqtt_latency_distribution.png)

Short description: MQTT round-trip latency distribution measured through the ping/pong path between the DUT and the edge server.

References:
- Figure file: [05_mqtt_latency_distribution.png](20260422_clean_dut_no_ina219_60s_v2/05_mqtt_latency_distribution.png)
- Latency data: [results_latency.csv](20260422_clean_dut_no_ina219_60s_v2/results_latency.csv)
- Serial log: [serial.log](20260422_clean_dut_no_ina219_60s_v2/logs/serial.log)

**Figure 6**

![LoRa latency](20260422_clean_dut_no_ina219_60s_v2/06_lora_latency.png)

Short description: LoRaWAN uplink latency across the captured windows, showing the slower and more variable long-range path compared with MQTT.

References:
- Figure file: [06_lora_latency.png](20260422_clean_dut_no_ina219_60s_v2/06_lora_latency.png)
- LoRa data: [results_lora.csv](20260422_clean_dut_no_ina219_60s_v2/results_lora.csv)
- Serial log: [serial.log](20260422_clean_dut_no_ina219_60s_v2/logs/serial.log)

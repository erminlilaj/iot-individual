# IoT Individual Assignment

Adaptive sampling pipeline on `ESP32-S3 / Heltec WiFi LoRa 32 V3` using `FreeRTOS`, `FFT`, `MQTT`, and optional `LoRaWAN`.

## Table Of Contents

- [Overview](#overview)
- [Assignment Coverage](#assignment-coverage)
- [System Architecture](#system-architecture)
- [Hardware Setup](#hardware-setup)
- [Repository Layout](#repository-layout)
- [Implementation Summary](#implementation-summary)
- [Setup And Run](#setup-and-run)
- [Logs And Results Layout](#logs-and-results-layout)
- [Expected Runtime Output](#expected-runtime-output)
- [Results Summary](#results-summary)
- [Evidence Gallery](#evidence-gallery)
- [Documentation Notes](#documentation-notes)
- [Current Limitations](#current-limitations)

## Overview

This project implements an end-to-end IoT node that generates a virtual sensor signal, estimates its dominant frequency with an FFT, adapts the sampling frequency, computes a `5 s` aggregate window, and transmits the result to a nearby edge server over `MQTT/WiFi`.

The same window result can also be sent through `LoRaWAN + TTN` when coverage and credentials are available.

The input signal is:

```text
s(t) = 2*sin(2*pi*3*t) + 4*sin(2*pi*5*t)
```

Core idea:

- sample the signal locally
- estimate the dominant frequency
- reduce the sampling rate to a Nyquist-safe value
- send one aggregate instead of many raw samples

## Assignment Coverage

| Requirement | Status | Notes |
| --- | --- | --- |
| Maximum sampling frequency | Done | Practical schedulable rate measured around `1000 Hz` in the FreeRTOS-based setup |
| Maximum input frequency | Done | FFT identifies the dominant component around `5 Hz` |
| Optimal sampling frequency | Done | Adaptive rate stabilizes around `10 Hz` |
| Aggregate over a window | Done | Mean computed over `5 s` windows |
| MQTT + WiFi edge delivery | Done | Python edge listener receives aggregate payloads |
| LoRaWAN + TTN cloud delivery | Implemented | Depends on TTN join and local radio coverage during demo |
| Communication cost evaluation | Done | Repo logs payload reduction and aggregate-only transmission |
| End-to-end latency evaluation | Done | MQTT RTT is logged in firmware and visible during runs |
| Energy evaluation | Partial but usable | Firmware includes a proxy model and the repo includes an INA219 monitor path |
| Bonus anomaly handling | Done | Noise mode, spike mode, Z-score, and Hampel logic are included |

## System Architecture

```text
virtual signal
    -> sampling task
    -> FFT analysis
    -> adaptive sampling controller
    -> 5 s aggregation window
    -> MQTT edge server
    -> optional LoRaWAN / TTN uplink
```

Main runtime tasks:

- `sampler_task`: samples the synthetic signal and fills the buffers
- `fft_task`: computes the spectrum and updates the sampling frequency
- `aggregator_task`: computes the `5 s` mean and triggers transmissions
- `mqtt_loop_task`: keeps the MQTT connection alive

## Hardware Setup

The project was developed around one `Heltec WiFi LoRa 32 V3` DUT and an optional second ESP32 used for INA219-based monitoring during external power-measurement experiments.

![Two ESP32 boards with INA219 wiring](images/two_esp32_ina219_setup.png)

## Repository Layout

```text
firmware/
  src/                 main ESP32 firmware
  platformio.ini       build environments and dependencies

tools/
  edge_server.py       MQTT edge listener
  serial_plotter.py    host-side plotting utility
  power_bridge.py      helper for measurement workflows
  monitor_esp32/       INA219 monitor firmware for plotting-oriented runs
  plot_sessions/       raw timestamped DUT capture sessions

images/
  screenshot evidence used in the demo / submission

source/
  results/             curated submission-style export of validated sessions
```

Most relevant files:

- `firmware/src/main.cpp`
- `firmware/src/tasks.cpp`
- `firmware/src/aggregator.cpp`
- `firmware/src/fft_analysis.cpp`
- `firmware/src/mqtt_client.cpp`
- `firmware/src/lorawan.cpp`
- `firmware/src/display.cpp`
- `tools/edge_server.py`

## Implementation Summary

### 1. Signal Generation

The signal is generated directly on the ESP32, which keeps the demo reproducible and avoids needing an external sensor source.

### 2. Maximum Sampling Frequency

The project uses a practical maximum of about `1000 Hz`, which matches the `1 ms` scheduling floor of the FreeRTOS-based implementation. This is more defensible than claiming a purely theoretical loop speed.

### 3. FFT And Adaptive Sampling

The firmware gathers a window of samples, runs an FFT, and estimates the dominant frequency near `5 Hz`. It then lowers the sampling rate to about `10 Hz`, which is consistent with a Nyquist-safe policy for this signal.

### 4. Aggregate Window

Every `5 s`, the firmware computes the mean of the most recent window and uses that as the transmitted result. This keeps the communication path simple and reduces data volume compared with sending raw samples.

### 5. MQTT Edge Communication

The aggregate is sent to a local MQTT broker over WiFi. The Python edge server receives the payload, logs it, and can be used to observe round-trip timing.

### 6. LoRaWAN Uplink

The same aggregate can be sent with LoRaWAN through TTN. This path is implemented in firmware, but successful live proof depends on join success and local network coverage.

### 7. Display And Monitoring

The DUT firmware includes an OLED dashboard with rotating pages for sampling frequency, FFT result, aggregate state, MQTT status, RTT, and LoRa join state.

The repository also contains a separate INA219 monitor path under `tools/monitor_esp32/` for external power measurements.

## Setup And Run

### 1. Prepare Local Credentials

Create the local config from the example:

```bash
cp firmware/src/config.h.example firmware/src/config.h
```

Then fill in:

- WiFi SSID and password
- MQTT broker host / port / topic settings
- TTN credentials if you want to test LoRaWAN

### 2. Build And Upload Firmware

```bash
cd firmware
~/.platformio/penv/bin/pio run -e heltec_wifi_lora_32_V3 -t upload --upload-port /dev/ttyUSB0
```

Other signal-mode environments are also available in `platformio.ini`.

### 3. Open The Serial Monitor

```bash
stty -F /dev/ttyUSB0 115200 raw cs8 -cstopb -parenb && cat /dev/ttyUSB0
```

### 4. Start The Edge Server

```bash
cd tools
python edge_server.py
```

The listener also accepts:

- `MQTT_BROKER_HOST`
- `MQTT_BROKER_PORT`

### 5. Optional Power Monitor Path

If you want external power traces, the repo also includes:

```bash
cd tools/monitor_esp32
~/.platformio/penv/bin/pio run -e monitor -t upload --upload-port /dev/ttyUSB0
```

This monitor firmware is intended for INA219-based measurement workflows.

### 6. Plot-Capture Workflow

To capture DUT-based report figures, flash one of the dedicated plot builds:

```bash
cd firmware
~/.platformio/penv/bin/pio run -e clean_plot_capture -t upload --upload-port /dev/ttyUSB0
```

If the plotting dependencies are not installed yet, create a temporary virtualenv:

```bash
python3 -m venv /tmp/plot-venv
/tmp/plot-venv/bin/pip install -r tools/requirements.txt
```

Then capture one session on the laptop:

```bash
/tmp/plot-venv/bin/python tools/plot_capture.py --port /dev/ttyUSB0 --max-windows 8 --session-name clean_dut
```

After the run finishes, generate the final figures and markdown ledger:

```bash
/tmp/plot-venv/bin/python tools/generate_session_plots.py tools/plot_sessions/<session_folder>
```

This produces:

- session-local PNG plots under `tools/plot_sessions/.../plots/`
- parsed CSV files for FFT, aggregation, MQTT send/receive, latency, and LoRa when present
- an updated `docs/plot_data_records.md` linking each figure to its source data

## Logs And Results Layout

The repo now has two layers of run artifacts:

- [`tools/plot_sessions/`](tools/plot_sessions): raw chronological capture sessions created by `tools/plot_capture.py`
- [`source/results/`](source/results): curated submission-facing exports copied from the validated session(s)

How to read them:

- `tools/edge_log.csv`: long-running MQTT listener history; useful for general inspection, but not the canonical report bundle
- `tools/plot_sessions/<timestamp>_<name>/serial.log`: raw DUT serial stream with timestamps
- `tools/plot_sessions/<timestamp>_<name>/*.csv`: parsed extracts from that same session
- `tools/plot_sessions/README.md`: index of capture sessions and which one is the final reference run
- `source/results/README.md`: top-level guide to the curated report bundle

Structured DUT prefixes in `serial.log`:

- `[FFT]`: dominant frequency estimate and updated adaptive sampling rate
- `[AGG]`: 5 s window aggregate
- `[MQTT]`: publish summary and compression ratio
- `[LATENCY]`: MQTT round-trip timing
- `[LoRa]`: uplink summary and end-to-end latency
- `[PLOT]` and `[PLOT-SAMPLES]`: raw FFT-window snapshot used for figure generation
- `[ENERGY]` and `[ANOMALY]`: supplemental instrumentation

If you only need the final validated evidence, start here:

- [`source/results/README.md`](source/results/README.md)
- [`source/results/20260422_clean_dut_no_ina219_60s_v2/SUMMARY.md`](source/results/20260422_clean_dut_no_ina219_60s_v2/SUMMARY.md)
- [`docs/plot_data_records.md`](docs/plot_data_records.md)

## Expected Runtime Output

Typical lines from the main firmware look like:

```text
[FFT] dominant = 5.00 Hz -> fs updated to 10.0 Hz
[AGG] win=3 mean=+0.0001 n=50 fs=10.0 Hz proc_us=...
[MQTT] #3 avg=0.0012 payload=6 B total=18 B ...
[LATENCY] rtt_ms=312
```

Typical OLED behavior on the DUT:

- splash screen at boot
- rotating dashboard pages
- frequency and mode information
- aggregate / window information
- MQTT / RTT / LoRa status

## Results Summary

The latest validated clean run is:

`source/results/20260422_clean_dut_no_ina219_60s_v2/`

It was exported from:

`tools/plot_sessions/20260422_120509_clean_dut_no_ina219_60s_v2/`

Validated metrics from that session:

| Metric | Value |
| --- | --- |
| Maximum schedulable sampling frequency | practical upper bound about `1000 Hz` in this design |
| Dominant signal frequency | `5.00 Hz` |
| Adaptive sampling frequency | `10.00 Hz` |
| FFT updates captured | `3` |
| Aggregation windows captured | `5` windows of `5 s` each |
| MQTT delivery | `5/5` sent and received at the edge listener |
| MQTT RTT | mean `630.6 ms`, max `891.0 ms` |
| LoRa uplinks | `5` captured |
| LoRa end-to-end latency | mean `11829.2 ms`, range `1589-24863 ms` |

Interpretation:

- the adaptive controller consistently settled at a Nyquist-safe `10 Hz`
- the `5 s` aggregation path produced stable windows with `n=50`
- MQTT is the strongest validated communication path for the demo
- LoRaWAN is implemented and successfully captured in the final clean session, but it remains slower and more environment-sensitive than MQTT
- energy should still be presented cautiously because the final canonical run used firmware with INA219 disabled and relied on the proxy instrumentation rather than external sensor validation

## Evidence Gallery

The following screenshots are already stored in [`images/`](images):

### Hardware

![Hardware setup](images/two_esp32_ina219_setup.png)

### Core MQTT Demo Path

Maximum schedulable sampling rate:

![Maximum sampling rate](images/02_max_sampling_rate.png)

FFT-based frequency detection and adaptive sampling:

![FFT adaptive sampling](images/03_fft_adaptive_sampling.png)

Edge server receiving aggregates:

![Edge server receiving values](images/04_edge_server_receiving.png)

Serial output showing MQTT publish and latency:

![MQTT latency serial](images/05_mqtt_latency_serial.png)

MQTT topics observed from `mosquitto_sub`:

![mosquitto_sub topics](images/06_mosquitto_sub_topics.png)

### Optional And Archival Evidence

LoRaWAN / TTN evidence from earlier successful sessions:

![TTN live data](images/07_ttn_live_data.png)

![LoRa serial uplink](images/08_lora_serial_uplink.png)

Serial plotting dashboard example:

![Serial plotter dashboard](images/09_serial_plotter_dashboard.png)

Anomaly detection output:

![Anomaly detection](images/11_anomaly_detection_spikes.png)

FFT contamination / filtering check:

![FFT contamination](images/12_fft_contamination.png)

Use the MQTT path as the main live demo. Keep the LoRa screenshots as implementation evidence, but describe them as archival if TTN is unavailable in the current environment.

## Documentation Notes

The repo now has three useful documentation layers:

- [`README.md`](README.md): submission-facing overview with setup, architecture, results, and image evidence
- [`docs/defense_notes.md`](docs/defense_notes.md): short speaking notes for the oral discussion
- [`docs/report_personal.md`](docs/report_personal.md): longer personal prep and explanation document

For consistency during presentation:

- present MQTT as the primary validated path
- present LoRaWAN as implemented but environment-dependent
- present energy as cautious and strongest when backed by the INA219 workflow
- reference the screenshots in `images/` rather than older claims from memory

## Current Limitations

- LoRaWAN proof is environment-dependent because TTN join can fail when coverage is poor
- energy claims should be presented cautiously unless backed by the external INA219 setup
- the strongest and most repeatable demo path is:

1. virtual signal generation
2. FFT frequency estimation
3. adaptive sampling
4. `5 s` mean
5. MQTT delivery to the local edge server

## References

- [arduinoFFT](https://github.com/kosme/arduinoFFT)
- [RadioLib](https://github.com/jgromes/RadioLib)
- [PubSubClient](https://github.com/knolleary/pubsubclient)
- [U8g2](https://github.com/olikraus/u8g2)

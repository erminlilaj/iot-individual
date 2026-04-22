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
- [Expected Runtime Output](#expected-runtime-output)
- [Results Summary](#results-summary)
- [Visualization Ideas](#visualization-ideas)
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

images/
  screenshot evidence used in the demo / submission
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

### 5. Optional Power Monitor Path

If you want external power traces, the repo also includes:

```bash
cd tools/monitor_esp32
~/.platformio/penv/bin/pio run -e monitor -t upload --upload-port /dev/ttyUSB0
```

This monitor firmware is intended for INA219-based measurement workflows.

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

Conservative values used in this repository:

| Metric | Value |
| --- | --- |
| Maximum schedulable sampling frequency | about `1000 Hz` |
| Dominant signal frequency | about `5 Hz` |
| Adaptive sampling frequency | about `10 Hz` |
| Aggregation window | `5 s` |
| MQTT payload per window | about `6 B` |
| LoRa payload per window | `2 B` |
| MQTT RTT | roughly `200-835 ms` in local tests |

Interpretation:

- adaptive sampling clearly reduces local sample-processing activity
- aggregation clearly reduces the number of transmitted values
- exact energy savings depend on how much WiFi, radio, and idle power dominate the board behavior

## Visualization Ideas

The most useful way to present the retrieved data is to separate it by question:

- `time series`: plot each received MQTT average against timestamp to show the `5 s` windows arriving over time
- `pipeline state`: show `dominant frequency`, `adaptive fs`, and `window sample count` on one chart to explain why the system converges near `10 Hz` and `n ~= 50`
- `latency`: use a small line plot or histogram of `[LATENCY] rtt_ms` to show MQTT round-trip stability
- `anomaly detection`: use a binary event timeline or confusion-matrix table for spike windows versus detected windows
- `power`: if the INA219 path is active, plot `current_mA` and `power_mW` together and annotate phase changes

Practical options with this repo:

- `tools/edge_server.py` + `tools/edge_log.csv`: best source for a simple timestamped MQTT trend chart
- `tools/serial_plotter.py`: quickest host-side live view for serial numeric streams
- `BetterSerialPlotter`: useful for live INA219 traces when the monitor firmware emits clean numeric channels
- `matplotlib` or `pandas`: best choice for a final static figure in the report, especially for MQTT averages, RTT, and anomaly markers

If you want the cleanest final figure set for submission, I would use:

1. one line chart of MQTT averages over time
2. one latency chart from `[LATENCY]`
3. one anomaly figure showing spike windows and detections
4. one optional INA219 chart only if you have a stable real capture

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

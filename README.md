# IoT Individual Assignment

Adaptive sampling on `ESP32-S3 / Heltec WiFi LoRa 32 V3` using `FreeRTOS`, `FFT`, `MQTT`, and optional `LoRaWAN`.

## What This Project Does

The device generates a virtual input signal:

```text
s(t) = 2*sin(2*pi*3*t) + 4*sin(2*pi*5*t)
```

Then it performs this pipeline:

1. Start from a higher sampling rate.
2. Collect a window of samples.
3. Run an FFT to estimate the dominant frequency.
4. Adapt the sampling frequency according to Nyquist.
5. Compute the average over a 5-second window.
6. Send the aggregate value to a nearby edge server using MQTT over WiFi.
7. Optionally send the same aggregate to TTN using LoRaWAN.

The main idea is simple: transmit one useful aggregate instead of many raw samples.

## Recommended Demo Path

For the class demo, the easiest path to explain is:

- virtual signal generation
- FFT-based frequency estimation
- adaptive sampling from `100 Hz` down to about `10 Hz`
- 5-second average
- MQTT transmission to the local edge server

`LoRaWAN` is implemented, but it is best presented as a secondary path because TTN join and radio coverage depend on the environment.

## Current Status

### Core features

- Virtual sinusoidal sensor: implemented
- FFT-based adaptive sampling: implemented
- 5-second average: implemented
- MQTT to local edge server: implemented
- LoRaWAN uplink: implemented, but environment-dependent during demo
- Per-window timing logs: implemented
- Data volume comparison: implemented

### Bonus features

- Noisy signal mode: implemented
- Spike injection mode: implemented
- Z-score / Hampel anomaly logic: implemented

These bonus parts are not required for the main defense and do not need to be the focus of the presentation.

## Measured Values Used In The Writeup

These are the conservative values currently used in the project:

| Metric | Value |
|--------|-------|
| Maximum schedulable sampling frequency | about `1000 Hz` |
| Dominant signal frequency | about `5 Hz` |
| Adaptive sampling frequency | about `10 Hz` |
| Aggregation window | `5 s` |
| MQTT payload per window | about `6 B` |
| LoRa payload per window | `2 B` |
| MQTT RTT | roughly `200-835 ms` in local tests |

Important note:
Energy numbers in this repository should be presented carefully. The firmware includes a software-side proxy and the repo also contains an external INA219 measurement path, but the safest oral claim is:

`adaptive sampling reduces processing activity and transmitted data volume; power savings depend on the real sleep/communication behavior of the board`

## Architecture

### FreeRTOS tasks

- `sampler_task`
  Samples the virtual signal and pushes values to the ring buffer and FFT buffer.
- `fft_task`
  Runs FFT on a completed sample block and updates the sampling frequency.
- `aggregator_task`
  Every 5 seconds computes the mean and sends it through MQTT and optionally LoRa.
- `mqtt_loop_task`
  Keeps the MQTT client alive in the background.

### Important files

- `firmware/src/main.cpp`
- `firmware/src/tasks.cpp`
- `firmware/src/aggregator.cpp`
- `firmware/src/mqtt_client.cpp`
- `firmware/src/lorawan.cpp`
- `firmware/src/sensor.cpp`
- `tools/edge_server.py`

## How To Run

### 1. Prepare credentials

Create `firmware/src/config.h` from `config.h.example` and fill in the WiFi and MQTT values.

If you want to test LoRaWAN too, also provide the TTN keys in the corresponding local header used by the firmware.

### 2. Flash the firmware

```bash
cd firmware
~/.platformio/penv/bin/pio run -e heltec_wifi_lora_32_V3 -t upload --upload-port /dev/ttyUSB0
```

### 3. Open the serial monitor

```bash
stty -F /dev/ttyUSB0 115200 raw cs8 -cstopb -parenb && cat /dev/ttyUSB0
```

### 4. Start the edge server

```bash
cd tools
python edge_server.py
```

## What To Look For In The Logs

Typical lines to show during the demo:

```text
[FFT]  dominant = 5.00 Hz  -> fs updated to 10.0 Hz
[MQTT] #3 avg=0.0012 payload=6 B total=18 B baseline=60000 B ratio=3333x
[LATENCY] rtt_ms=312
[AGG]  win=3  mean=+0.0001  n=50  fs=10.0 Hz  proc_us=...
```

These lines are enough to explain:

- the signal frequency estimation
- the adaptive rate
- the aggregate computation
- the edge transmission
- the reduced transmitted volume

## Performance Discussion

### Sampling

The project uses a practical ceiling of about `1000 Hz` because the sampling task uses `vTaskDelay()` and the scheduler tick gives a `1 ms` floor. This is easier to defend than claiming a much higher theoretical number that the full FreeRTOS system does not actually sustain.

### Communication volume

The strongest communication argument is local:

- without aggregation, many raw samples would need to be sent
- with aggregation, only one value is sent every 5 seconds

This clearly reduces transmitted data volume.

### Energy

Energy should be described cautiously:

- adaptive sampling reduces CPU activity related to sampling
- aggregation reduces communication payload volume
- real energy savings depend on WiFi, radio, and sleep behavior

If asked for exact battery-life claims, it is better to say that the repository includes an evaluation path, but the safest conclusion is qualitative unless measured directly with external instrumentation.

## Bonus

The repository also contains:

- noisy and spike-contaminated signal modes
- anomaly-related helper code
- FFT contamination checks
- INA219 monitor support

These features are useful for experimentation, but they are not necessary to understand the core assignment.

## Defense Strategy

If time is short, explain only this:

1. I generate a known sinusoidal signal on the device.
2. I sample it.
3. I run FFT to estimate the dominant frequency.
4. I adapt the sampling rate to roughly `2 * f_max`.
5. I compute the mean over `5 s`.
6. I send the mean via MQTT to a nearby edge server.

That is enough to defend the main project.

## References

- [ArduinoFFT](https://github.com/kosme/arduinoFFT)
- [RadioLib](https://github.com/jgromes/RadioLib)
- [PubSubClient](https://github.com/knolleary/pubsubclient)
- [U8g2](https://github.com/olikraus/u8g2)

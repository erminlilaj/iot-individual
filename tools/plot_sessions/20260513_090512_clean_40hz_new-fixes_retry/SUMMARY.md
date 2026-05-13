# Clean 40 Hz New-Fixes Capture

Session captured on `2026-05-13` from `/dev/ttyUSB0` after flashing `clean_plot_capture`.

## What Was Confirmed

| Metric | Result |
| --- | ---: |
| FFT updates parsed | `11` |
| Dominant frequency | `5.02 Hz` every parsed FFT update |
| Adaptive policy | `8.0x` every parsed FFT update |
| Adaptive sampling rate | `40.0 Hz` every parsed FFT update |
| Aggregation windows parsed | `14` |
| Samples per aggregation window | `200` every parsed window |
| Aggregation mean range | `-0.0013` to `+0.0014` |
| Aggregation processing time | `95.526 ms` to `127.775 ms` |
| Panic / reboot signatures | none found |

This validates the new conservative adaptive policy:

```text
dominant ~= 5.02 Hz
fs = round_to_5Hz(8 * dominant) = 40.0 Hz
window_n = 40 Hz * 5 s = 200 samples
```

## What Was Not Confirmed

- No `[MQTT]` or `[LATENCY]` lines appeared during this capture.
- The local Mosquitto broker on port `1883` was already running in local-only mode, so the ESP32 may not have been able to reach it over WiFi.
- LoRaWAN still did not join during the capture: `9` observed attempts failed with `-1116`.

Use this session as fresh evidence for FFT/adaptive sampling and aggregation. Do not use it as fresh MQTT or LoRa delivery evidence.

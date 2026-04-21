#!/usr/bin/env python3
"""
Real-time serial plotter for the IoT adaptive sampling pipeline.

Parses [AGG], [LoRa], [ENERGY], [ANOMALY], [LATENCY], and [POWER] lines from
the ESP32 serial port and displays six live-updating panels:
  1. Rolling window mean over time
  2. Adaptive sampling frequency over time
  3. Anomaly detector TPR/FPR bar chart (Z-score + Hampel k=5/20/50)
  4. Per-window processing time (ms)
  5. End-to-end latency — LoRa e2e and MQTT RTT side-by-side
  6. INA219 power draw (mW) over time

Usage:
    pip install pyserial matplotlib paho-mqtt
    python tools/serial_plotter.py --port /dev/ttyUSB0 --baud 115200
"""

import argparse
import collections
import re
import threading
import time

import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import matplotlib.gridspec as gridspec

# ── Regex patterns for each log prefix ────────────────────────────────────────

RE_AGG = re.compile(
    r'\[AGG\].*win=(\d+).*mean=([+\-\d.]+).*n=(\d+).*fs=([\d.]+).*proc_us=(\d+)'
)
RE_LORA = re.compile(
    r'\[LoRa\].*e2e_latency=(\d+) ms'
)
RE_ENERGY = re.compile(
    r'\[ENERGY\].*savings=([\d.]+)%'
)
RE_ANOMALY_Z = re.compile(
    r'\[ANOMALY\] Z-score:.*TPR=([\d.]+).*FPR=([\d.]+)'
)
RE_ANOMALY_H = re.compile(
    r'\[ANOMALY\] Hampel (k=\d+)\s*:.*TPR=([\d.]+).*FPR=([\d.]+)'
)
RE_FFT_CONTAM = re.compile(
    r'\[FFT-CONTAM\].*raw_peak=([\d.]+).*filtered_peak=([\d.]+)'
)
RE_LATENCY = re.compile(
    r'\[LATENCY\].*rtt_ms=(\d+)'
)
RE_POWER = re.compile(
    r'\[POWER\].*?([-\d.]+)mA\s+([\d.]+)mW'
)

# ── Shared data (filled by serial reader thread) ───────────────────────────────

MAXLEN = 120  # keep last 120 data points (~10 min at one per 5 s)

data = {
    'win':       collections.deque(maxlen=MAXLEN),
    'mean':      collections.deque(maxlen=MAXLEN),
    'fs':        collections.deque(maxlen=MAXLEN),
    'proc_ms':   collections.deque(maxlen=MAXLEN),   # proc_us → ms for readability
    'e2e_ms':    collections.deque(maxlen=MAXLEN),
    'mqtt_rtt':  collections.deque(maxlen=MAXLEN),
    'savings':   collections.deque(maxlen=MAXLEN),
    'power_mw':  collections.deque(maxlen=MAXLEN),
    # Anomaly: latest TPR/FPR for each detector
    'z_tpr': 0.0, 'z_fpr': 0.0,
    'h5_tpr': 0.0, 'h5_fpr': 0.0,
    'h20_tpr': 0.0, 'h20_fpr': 0.0,
    'h50_tpr': 0.0, 'h50_fpr': 0.0,
    # FFT contamination: latest pair
    'fft_raw': None, 'fft_flt': None,
}
lock = threading.Lock()


def serial_reader(port: str, baud: int):
    """Background thread: reads lines from serial and populates `data`."""
    while True:
        ser = None
        try:
            # Open without port arg so pyserial doesn't touch DTR/RTS on construction
            ser = serial.Serial()
            ser.port = port
            ser.baudrate = baud
            ser.timeout = 1
            ser.dsrdtr = False
            ser.rtscts = False
            ser.open()
            print(f"[plotter] Connected to {port} @ {baud}")
            timeout_streak = 0
            while True:
                raw = ser.readline()
                if not raw:
                    timeout_streak += 1
                    if timeout_streak % 5 == 0:
                        print(f"[plotter] No data for {timeout_streak}s — ESP32 silent?")
                    continue
                timeout_streak = 0
                line = raw.decode('utf-8', errors='replace').strip()
                if line:
                    print(f"[serial] {line}")

                    m = RE_AGG.search(line)
                    if m:
                        print(f"[match AGG] win={m.group(1)} mean={m.group(2)} fs={m.group(4)} proc_us={m.group(5)}")
                        with lock:
                            data['win'].append(int(m.group(1)))
                            data['mean'].append(float(m.group(2)))
                            data['fs'].append(float(m.group(4)))
                            data['proc_ms'].append(int(m.group(5)) / 1000.0)
                        continue

                    m = RE_LORA.search(line)
                    if m:
                        print(f"[match LoRa] e2e={m.group(1)} ms")
                        with lock:
                            data['e2e_ms'].append(int(m.group(1)))
                        continue

                    m = RE_ENERGY.search(line)
                    if m:
                        print(f"[match ENERGY] savings={m.group(1)}%")
                        with lock:
                            data['savings'].append(float(m.group(1)))
                        continue

                    m = RE_ANOMALY_Z.search(line)
                    if m:
                        print(f"[match ANOMALY-Z] TPR={m.group(1)} FPR={m.group(2)}")
                        with lock:
                            data['z_tpr'] = float(m.group(1))
                            data['z_fpr'] = float(m.group(2))
                        continue

                    m = RE_ANOMALY_H.search(line)
                    if m:
                        k_label = m.group(1)  # "k=5", "k=20", "k=50"
                        tpr, fpr = float(m.group(2)), float(m.group(3))
                        print(f"[match ANOMALY-H] {k_label} TPR={tpr} FPR={fpr}")
                        with lock:
                            if k_label == 'k=5':
                                data['h5_tpr'], data['h5_fpr'] = tpr, fpr
                            elif k_label == 'k=20':
                                data['h20_tpr'], data['h20_fpr'] = tpr, fpr
                            elif k_label == 'k=50':
                                data['h50_tpr'], data['h50_fpr'] = tpr, fpr
                        continue

                    m = RE_FFT_CONTAM.search(line)
                    if m:
                        print(f"[match FFT-CONTAM] raw={m.group(1)} flt={m.group(2)}")
                        with lock:
                            data['fft_raw'] = float(m.group(1))
                            data['fft_flt'] = float(m.group(2))
                        continue

                    m = RE_LATENCY.search(line)
                    if m:
                        print(f"[match LATENCY] rtt={m.group(1)} ms")
                        with lock:
                            data['mqtt_rtt'].append(int(m.group(1)))
                        continue

                    m = RE_POWER.search(line)
                    if m:
                        print(f"[match POWER] mA={m.group(1)} mW={m.group(2)}")
                        with lock:
                            data['power_mw'].append(float(m.group(2)))
                        continue

        except serial.SerialException as e:
            print(f"[plotter] Serial error: {e} — retrying in 3 s")
            time.sleep(3)
        finally:
            if ser and ser.is_open:
                ser.close()


# ── Plot setup ─────────────────────────────────────────────────────────────────

def build_figure():
    fig = plt.figure(figsize=(14, 11))
    fig.suptitle("IoT Adaptive Sampling Pipeline — Live Monitor", fontsize=13)
    gs = gridspec.GridSpec(3, 3, figure=fig, hspace=0.50, wspace=0.35)

    ax_mean    = fig.add_subplot(gs[0, 0:2])   # wide: rolling mean
    ax_fs      = fig.add_subplot(gs[1, 0:2])   # wide: adaptive fs
    ax_latency = fig.add_subplot(gs[2, 0:2])   # wide: LoRa e2e + MQTT RTT
    ax_anom    = fig.add_subplot(gs[0, 2])     # anomaly TPR/FPR bars
    ax_timing  = fig.add_subplot(gs[1, 2])     # per-window proc time
    ax_power   = fig.add_subplot(gs[2, 2])     # INA219 power draw

    ax_mean.set_title("5-second Window Mean")
    ax_mean.set_xlabel("Window #")
    ax_mean.set_ylabel("Mean amplitude")
    ax_mean.axhline(0, color='gray', linewidth=0.5, linestyle='--')
    line_mean, = ax_mean.plot([], [], 'b-o', markersize=3, linewidth=1)

    ax_fs.set_title("Adaptive Sampling Rate")
    ax_fs.set_xlabel("Window #")
    ax_fs.set_ylabel("fs (Hz)")
    ax_fs.axhline(10, color='green', linewidth=0.8, linestyle='--', label='Nyquist 10 Hz')
    ax_fs.axhline(100, color='red', linewidth=0.8, linestyle='--', label='Max 100 Hz')
    ax_fs.legend(fontsize=7)
    line_fs, = ax_fs.plot([], [], 'r-o', markersize=3, linewidth=1)

    detector_labels = ['Z-score', 'Hampel\nk=5', 'Hampel\nk=20', 'Hampel\nk=50']
    x_pos = range(len(detector_labels))
    bars_tpr = ax_anom.bar([x - 0.2 for x in x_pos], [0]*4, 0.35,
                           label='TPR', color='steelblue')
    bars_fpr = ax_anom.bar([x + 0.2 for x in x_pos], [0]*4, 0.35,
                           label='FPR', color='tomato')
    ax_anom.set_title("Anomaly Detection\nTPR / FPR")
    ax_anom.set_xticks(list(x_pos))
    ax_anom.set_xticklabels(detector_labels, fontsize=7)
    ax_anom.set_ylim(0, 1.05)
    ax_anom.set_ylabel("Rate")
    ax_anom.legend(fontsize=7)

    ax_timing.set_title("Per-Window Processing Time")
    ax_timing.set_xlabel("Window #")
    ax_timing.set_ylabel("ms")
    line_proc, = ax_timing.plot([], [], 'g-o', markersize=3, linewidth=1)

    ax_latency.set_title("End-to-End Latency")
    ax_latency.set_xlabel("Measurement #")
    ax_latency.set_ylabel("ms")
    line_lora_lat, = ax_latency.plot([], [], 'b-o', markersize=3, linewidth=1, label='LoRa e2e')
    line_mqtt_rtt, = ax_latency.plot([], [], 'r-o', markersize=3, linewidth=1, label='MQTT RTT')
    ax_latency.legend(fontsize=7)

    ax_power.set_title("INA219 Power Draw")
    ax_power.set_xlabel("Sample #")
    ax_power.set_ylabel("mW")
    ax_power.axhline(0, color='gray', linewidth=0.5, linestyle='--')
    line_power, = ax_power.plot([], [], 'm-', linewidth=1)

    return fig, (ax_mean, ax_fs, ax_anom, ax_timing, ax_latency, ax_power), \
           (line_mean, line_fs, line_proc, line_lora_lat, line_mqtt_rtt, line_power), \
           (bars_tpr, bars_fpr)


def make_updater(axes, lines, bars):
    ax_mean, ax_fs, ax_anom, ax_timing, ax_latency, ax_power = axes
    line_mean, line_fs, line_proc, line_lora_lat, line_mqtt_rtt, line_power = lines
    bars_tpr, bars_fpr = bars

    def update(_frame):
        with lock:
            wins      = list(data['win'])
            means     = list(data['mean'])
            fs_vals   = list(data['fs'])
            proc_ms   = list(data['proc_ms'])
            e2e_ms    = list(data['e2e_ms'])
            mqtt_rtt  = list(data['mqtt_rtt'])
            power_mw  = list(data['power_mw'])
            tpr_vals  = [data['z_tpr'],  data['h5_tpr'],  data['h20_tpr'],  data['h50_tpr']]
            fpr_vals  = [data['z_fpr'],  data['h5_fpr'],  data['h20_fpr'],  data['h50_fpr']]
            fft_raw   = data['fft_raw']
            fft_flt   = data['fft_flt']

        if wins:
            line_mean.set_data(wins, means)
            ax_mean.relim(); ax_mean.autoscale_view()

            line_fs.set_data(wins, fs_vals)
            ax_fs.relim(); ax_fs.autoscale_view()

        if proc_ms:
            line_proc.set_data(list(range(len(proc_ms))), proc_ms)
            ax_timing.relim(); ax_timing.autoscale_view()

        if e2e_ms:
            line_lora_lat.set_data(list(range(len(e2e_ms))), e2e_ms)
            ax_latency.relim(); ax_latency.autoscale_view()

        if mqtt_rtt:
            line_mqtt_rtt.set_data(list(range(len(mqtt_rtt))), mqtt_rtt)
            ax_latency.relim(); ax_latency.autoscale_view()

        if power_mw:
            line_power.set_data(list(range(len(power_mw))), power_mw)
            ax_power.relim(); ax_power.autoscale_view()

        for bar, val in zip(bars_tpr, tpr_vals):
            bar.set_height(val)
        for bar, val in zip(bars_fpr, fpr_vals):
            bar.set_height(val)

        if fft_raw is not None and fft_flt is not None:
            ax_anom.set_title(
                f"Anomaly TPR/FPR\nFFT: raw={fft_raw:.1f} Hz  flt={fft_flt:.1f} Hz",
                fontsize=7
            )

        return (line_mean, line_fs, line_proc, line_lora_lat,
                line_mqtt_rtt, line_power, *bars_tpr, *bars_fpr)

    return update


def main():
    parser = argparse.ArgumentParser(description="IoT serial plotter")
    parser.add_argument('--port', default='/dev/ttyUSB0', help='Serial port')
    parser.add_argument('--baud', type=int, default=115200, help='Baud rate')
    args = parser.parse_args()

    t = threading.Thread(target=serial_reader, args=(args.port, args.baud), daemon=True)
    t.start()

    fig, axes, lines, bars = build_figure()
    updater = make_updater(axes, lines, bars)
    ani = animation.FuncAnimation(fig, updater, interval=1000, blit=False, cache_frame_data=False)

    plt.show()


if __name__ == '__main__':
    main()

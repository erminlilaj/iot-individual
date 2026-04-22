#!/usr/bin/env python3
"""
Generate report-ready figures and a markdown data ledger from one capture session.
"""

from __future__ import annotations

import argparse
import csv
import os
from pathlib import Path

os.environ.setdefault("MPLCONFIGDIR", "/tmp/matplotlib")

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def load_csv(path: Path) -> list[dict[str, str]]:
    if not path.exists():
        return []
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def select_reference_plot_sequence(rows: list[dict[str, str]]) -> tuple[int, float, np.ndarray, np.ndarray]:
    if not rows:
        raise ValueError("plot_samples.csv is empty")
    grouped: dict[int, list[dict[str, str]]] = {}
    for row in rows:
        grouped.setdefault(int(row["seq"]), []).append(row)

    seq, selected = max(
        grouped.items(),
        key=lambda item: (
            float(item[1][0]["fs_hz"]),
            -item[0],
        ),
    )
    selected.sort(key=lambda row: int(row["sample_index"]))
    fs_hz = float(selected[0]["fs_hz"])
    t = np.array([float(row["t_seconds"]) for row in selected], dtype=float)
    y = np.array([float(row["value"]) for row in selected], dtype=float)
    return seq, fs_hz, t, y


def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def plot_waveform(out_path: Path, seq: int, fs_hz: float, t: np.ndarray, y: np.ndarray) -> dict[str, str]:
    fig, ax = plt.subplots(figsize=(10, 4.6))
    ax.plot(t, y, linewidth=1.5, color="#1f6f8b")
    ax.axhline(0, color="0.7", linewidth=0.8)
    ax.set_title(f"DUT waveform snapshot from raw FFT window (seq {seq})")
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Signal value")
    ax.grid(alpha=0.25)
    fig.tight_layout()
    fig.savefig(out_path, dpi=180)
    plt.close(fig)
    return {
        "samples": str(len(y)),
        "fs_hz": f"{fs_hz:.3f}",
        "y_min": f"{np.min(y):+.4f}",
        "y_max": f"{np.max(y):+.4f}",
    }


def plot_spectrum(out_path: Path, seq: int, fs_hz: float, y: np.ndarray) -> dict[str, str]:
    window = np.hamming(len(y))
    spectrum = np.fft.rfft(y * window)
    freqs = np.fft.rfftfreq(len(y), d=1.0 / fs_hz)
    mags = np.abs(spectrum)

    peak_bins = np.argsort(mags[1:])[-2:] + 1
    peak_bins = peak_bins[np.argsort(freqs[peak_bins])]
    peak_labels = [f"{freqs[idx]:.2f} Hz" for idx in peak_bins]

    fig, ax = plt.subplots(figsize=(10, 4.6))
    ax.plot(freqs, mags, linewidth=1.4, color="#d17a22")
    for idx in peak_bins:
        ax.scatter(freqs[idx], mags[idx], color="#b22222", s=35, zorder=3)
        ax.annotate(
            f"{freqs[idx]:.2f} Hz",
            (freqs[idx], mags[idx]),
            textcoords="offset points",
            xytext=(4, 6),
            fontsize=8,
        )
    ax.set_title(f"FFT magnitude spectrum from DUT window (seq {seq})")
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("Magnitude")
    ax.grid(alpha=0.25)
    fig.tight_layout()
    fig.savefig(out_path, dpi=180)
    plt.close(fig)
    return {
        "samples": str(len(y)),
        "fs_hz": f"{fs_hz:.3f}",
        "peak_1": peak_labels[0] if peak_labels else "n/a",
        "peak_2": peak_labels[1] if len(peak_labels) > 1 else "n/a",
    }


def plot_adaptive_fs(out_path: Path, fft_rows: list[dict[str, str]]) -> dict[str, str]:
    idx = np.arange(1, len(fft_rows) + 1)
    dominant = np.array([float(row["dominant_hz"]) for row in fft_rows], dtype=float)
    fs_vals = np.array([float(row["fs_hz"]) for row in fft_rows], dtype=float)

    fig, ax = plt.subplots(figsize=(10, 4.6))
    ax.step(idx, fs_vals, where="post", linewidth=1.8, color="#116466", label="adaptive fs")
    ax.plot(idx, 2.0 * dominant, "o--", linewidth=1.0, markersize=4, color="#ff7f11", label="2 x dominant")
    ax.axhline(10.0, color="0.5", linestyle="--", linewidth=0.9, label="10 Hz target")
    ax.set_title("Adaptive sampling rate over FFT updates")
    ax.set_xlabel("FFT update index")
    ax.set_ylabel("Frequency (Hz)")
    ax.grid(alpha=0.25)
    ax.legend()
    fig.tight_layout()
    fig.savefig(out_path, dpi=180)
    plt.close(fig)
    return {
        "fft_updates": str(len(fft_rows)),
        "dominant_mean_hz": f"{np.mean(dominant):.3f}",
        "fs_mean_hz": f"{np.mean(fs_vals):.3f}",
        "fs_min_hz": f"{np.min(fs_vals):.3f}",
        "fs_max_hz": f"{np.max(fs_vals):.3f}",
    }


def plot_aggregate_delivery(
    out_path: Path,
    agg_rows: list[dict[str, str]],
    send_rows: list[dict[str, str]],
    rx_rows: list[dict[str, str]],
) -> dict[str, str]:
    send_values = np.array([float(row["avg"]) for row in send_rows], dtype=float)
    rx_values = np.array([float(row["payload"]) for row in rx_rows if row["topic"] == "eri/iot/average"], dtype=float)
    n = min(len(send_values), len(rx_values))
    x = np.arange(1, n + 1)

    fig, ax = plt.subplots(figsize=(10, 4.6))
    if agg_rows:
        agg_x = np.array([int(row["window"]) for row in agg_rows], dtype=int)
        agg_y = np.array([float(row["mean"]) for row in agg_rows], dtype=float)
        ax.plot(agg_x, agg_y, color="#7a9e9f", linewidth=1.2, alpha=0.7, label="window mean")
    if n > 0:
        ax.plot(x, send_values[:n], "o-", linewidth=1.2, markersize=4, color="#2a9d8f", label="MQTT sent avg")
        ax.scatter(x, rx_values[:n], color="#e76f51", s=32, label="edge received avg", zorder=3)
    ax.axhline(0, color="0.7", linewidth=0.8)
    ax.set_title("5 s aggregate values across the MQTT path")
    ax.set_xlabel("Message / window index")
    ax.set_ylabel("Average value")
    ax.grid(alpha=0.25)
    ax.legend()
    fig.tight_layout()
    fig.savefig(out_path, dpi=180)
    plt.close(fig)
    return {
        "agg_windows": str(len(agg_rows)),
        "mqtt_sent_messages": str(len(send_rows)),
        "mqtt_received_messages": str(len(rx_values)),
        "matched_messages": str(n),
    }


def plot_latency_distribution(out_path: Path, latency_rows: list[dict[str, str]]) -> dict[str, str]:
    vals = np.array([float(row["rtt_ms"]) for row in latency_rows], dtype=float)

    fig, axes = plt.subplots(1, 2, figsize=(10.5, 4.6), gridspec_kw={"width_ratios": [2.8, 1]})
    axes[0].hist(vals, bins=min(8, max(4, len(vals))), color="#4c78a8", alpha=0.85)
    axes[0].set_title("MQTT RTT histogram")
    axes[0].set_xlabel("RTT (ms)")
    axes[0].set_ylabel("Count")
    axes[0].grid(alpha=0.2)

    axes[1].boxplot(vals, vert=True, patch_artist=True, boxprops={"facecolor": "#f4a261"})
    axes[1].set_title("MQTT RTT spread")
    axes[1].set_ylabel("RTT (ms)")
    axes[1].grid(alpha=0.2)

    fig.suptitle("MQTT latency distribution")
    fig.tight_layout()
    fig.savefig(out_path, dpi=180)
    plt.close(fig)
    return {
        "rtt_samples": str(len(vals)),
        "rtt_mean_ms": f"{np.mean(vals):.1f}",
        "rtt_median_ms": f"{np.median(vals):.1f}",
        "rtt_p95_ms": f"{np.percentile(vals, 95):.1f}",
        "rtt_max_ms": f"{np.max(vals):.1f}",
    }


def plot_lora_latency(out_path: Path, lora_rows: list[dict[str, str]]) -> dict[str, str]:
    idx = np.array([int(row["uplink_index"]) for row in lora_rows], dtype=int)
    mean_vals = np.array([float(row["mean"]) for row in lora_rows], dtype=float)
    e2e_ms = np.array([float(row["e2e_latency_ms"]) for row in lora_rows], dtype=float)

    fig, axes = plt.subplots(2, 1, figsize=(10, 6.4), sharex=True, gridspec_kw={"height_ratios": [2, 1.3]})
    axes[0].plot(idx, e2e_ms, "o-", linewidth=1.5, markersize=5, color="#7c4dff")
    axes[0].set_title("LoRa uplink end-to-end latency")
    axes[0].set_ylabel("Latency (ms)")
    axes[0].grid(alpha=0.25)

    axes[1].plot(idx, mean_vals, "s-", linewidth=1.2, markersize=4, color="#2a9d8f")
    axes[1].axhline(0, color="0.7", linewidth=0.8)
    axes[1].set_xlabel("LoRa uplink index")
    axes[1].set_ylabel("Mean value")
    axes[1].grid(alpha=0.25)

    fig.tight_layout()
    fig.savefig(out_path, dpi=180)
    plt.close(fig)
    return {
        "uplink_count": str(len(lora_rows)),
        "latency_mean_ms": f"{np.mean(e2e_ms):.1f}",
        "latency_min_ms": f"{np.min(e2e_ms):.1f}",
        "latency_max_ms": f"{np.max(e2e_ms):.1f}",
    }


def assess_categories(
    waveform_meta: dict[str, str],
    spectrum_meta: dict[str, str],
    fs_rows: list[dict[str, str]],
    agg_rows: list[dict[str, str]],
    send_rows: list[dict[str, str]],
    rx_rows: list[dict[str, str]],
    latency_rows: list[dict[str, str]],
    lora_rows: list[dict[str, str]],
) -> list[dict[str, str]]:
    results: list[dict[str, str]] = []

    y_max = max(abs(float(waveform_meta["y_min"])), abs(float(waveform_meta["y_max"])))
    waveform_status = "normal" if 4.0 <= y_max <= 6.5 else "abnormal"
    results.append({
        "category": "waveform",
        "status": waveform_status,
        "reason": f"peak amplitude about {y_max:.2f}, expected around the clean signal range",
    })

    dominant_vals = [float(row["dominant_hz"]) for row in fs_rows]
    dominant_mean = float(np.mean(dominant_vals))
    fft_status = "normal" if abs(dominant_mean - 5.0) <= 0.2 else "abnormal"
    results.append({
        "category": "fft_dominant_frequency",
        "status": fft_status,
        "reason": f"mean dominant frequency {dominant_mean:.2f} Hz, expected about 5 Hz",
    })

    fs_vals = [float(row["fs_hz"]) for row in fs_rows]
    fs_mean = float(np.mean(fs_vals))
    adaptive_status = "normal" if all(9.5 <= val <= 10.5 for val in fs_vals) else "abnormal"
    results.append({
        "category": "adaptive_sampling_rate",
        "status": adaptive_status,
        "reason": f"adaptive fs mean {fs_mean:.2f} Hz, expected about 10 Hz after Nyquist adaptation",
    })

    ns = [int(row["sample_count"]) for row in agg_rows]
    means = [abs(float(row["mean"])) for row in agg_rows]
    agg_status = "normal" if all(48 <= n <= 52 for n in ns) and float(np.mean(means)) <= 0.05 else "abnormal"
    results.append({
        "category": "aggregation_window",
        "status": agg_status,
        "reason": f"sample counts {min(ns)}-{max(ns)} and mean magnitude avg {np.mean(means):.4f}",
    })

    rx_avg_rows = [row for row in rx_rows if row["topic"] == "eri/iot/average"]
    matched = min(len(send_rows), len(rx_avg_rows))
    delivery_ratio = matched / len(send_rows) if send_rows else 0.0
    mqtt_delivery_status = "normal" if delivery_ratio >= 0.95 else "abnormal"
    results.append({
        "category": "mqtt_delivery",
        "status": mqtt_delivery_status,
        "reason": f"matched {matched}/{len(send_rows)} aggregate publishes at the edge listener",
    })

    rtts = [float(row["rtt_ms"]) for row in latency_rows]
    latency_status = "normal" if rtts and max(rtts) <= 2500.0 else "abnormal"
    results.append({
        "category": "mqtt_latency",
        "status": latency_status,
        "reason": f"RTT mean {np.mean(rtts):.1f} ms, max {np.max(rtts):.1f} ms",
    })

    if lora_rows:
        lora_lat = [float(row["e2e_latency_ms"]) for row in lora_rows]
        lora_status = "normal" if all(0.0 < val <= 30000.0 for val in lora_lat) else "abnormal"
        results.append({
            "category": "lora_uplink",
            "status": lora_status,
            "reason": f"{len(lora_rows)} uplinks captured, latency range {min(lora_lat):.0f}-{max(lora_lat):.0f} ms which is high but still realistic for LoRaWAN",
        })
    else:
        results.append({
            "category": "lora_uplink",
            "status": "abnormal",
            "reason": "no successful LoRa uplinks captured in this session",
        })

    return results


def build_markdown(
    session_dir: Path,
    plots_dir: Path,
    waveform_meta: dict[str, str],
    spectrum_meta: dict[str, str],
    fs_meta: dict[str, str],
    agg_meta: dict[str, str],
    latency_meta: dict[str, str],
    lora_meta: dict[str, str],
    assessments: list[dict[str, str]],
) -> str:
    assessment_lines = "\n".join(
        f"- `{item['category']}`: **{item['status']}** — {item['reason']}"
        for item in assessments
    )
    return f"""# Plot Data Records

Session folder: `{session_dir}`

This file records the exact data sources used to generate the report/defense plots for the latest captured DUT session.

## Figure 1 — DUT waveform snapshot

- Output: `{plots_dir / '01_waveform_snapshot.png'}`
- Sources: `plot_samples.csv`, `metadata.json`
- Samples used: `{waveform_meta['samples']}`
- Sampling rate: `{waveform_meta['fs_hz']} Hz`
- Value range: `{waveform_meta['y_min']}` to `{waveform_meta['y_max']}`

## Figure 2 — DUT FFT magnitude spectrum

- Output: `{plots_dir / '02_fft_spectrum.png'}`
- Sources: `plot_samples.csv`
- Samples used: `{spectrum_meta['samples']}`
- Sampling rate: `{spectrum_meta['fs_hz']} Hz`
- Strongest peaks: `{spectrum_meta['peak_1']}`, `{spectrum_meta['peak_2']}`

## Figure 3 — Adaptive sampling rate over time

- Output: `{plots_dir / '03_adaptive_fs.png'}`
- Sources: `fft.csv`
- FFT updates: `{fs_meta['fft_updates']}`
- Mean dominant frequency: `{fs_meta['dominant_mean_hz']} Hz`
- Mean adaptive sampling rate: `{fs_meta['fs_mean_hz']} Hz`
- Adaptive sampling range: `{fs_meta['fs_min_hz']} Hz` to `{fs_meta['fs_max_hz']} Hz`

## Figure 4 — Aggregate values across the MQTT path

- Output: `{plots_dir / '04_aggregate_mqtt_path.png'}`
- Sources: `agg.csv`, `mqtt_send.csv`, `mqtt_rx.csv`
- Aggregate windows logged: `{agg_meta['agg_windows']}`
- MQTT sent messages: `{agg_meta['mqtt_sent_messages']}`
- MQTT received messages: `{agg_meta['mqtt_received_messages']}`
- Matched sent/received messages plotted: `{agg_meta['matched_messages']}`

## Figure 5 — MQTT latency distribution

- Output: `{plots_dir / '05_mqtt_latency_distribution.png'}`
- Sources: `latency.csv`
- RTT samples: `{latency_meta['rtt_samples']}`
- Mean RTT: `{latency_meta['rtt_mean_ms']} ms`
- Median RTT: `{latency_meta['rtt_median_ms']} ms`
- p95 RTT: `{latency_meta['rtt_p95_ms']} ms`
- Max RTT: `{latency_meta['rtt_max_ms']} ms`

## Figure 6 — LoRa uplink latency

- Output: `{plots_dir / '06_lora_latency.png'}`
- Sources: `lora.csv`
- LoRa uplinks captured: `{lora_meta['uplink_count']}`
- Mean LoRa e2e latency: `{lora_meta['latency_mean_ms']} ms`
- Min LoRa e2e latency: `{lora_meta['latency_min_ms']} ms`
- Max LoRa e2e latency: `{lora_meta['latency_max_ms']} ms`

## Assessment By Data Category

These status labels are inferred from the project's expected clean-mode behavior.

{assessment_lines}
"""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate plots from one capture session")
    parser.add_argument("session_dir", help="Path under tools/plot_sessions/")
    parser.add_argument(
        "--records-path",
        default="docs/plot_data_records.md",
        help="Markdown record file to overwrite with the latest plot data ledger",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    session_dir = Path(args.session_dir).resolve()
    plots_dir = session_dir / "plots"
    ensure_dir(plots_dir)

    fft_rows = load_csv(session_dir / "fft.csv")
    agg_rows = load_csv(session_dir / "agg.csv")
    latency_rows = load_csv(session_dir / "latency.csv")
    send_rows = load_csv(session_dir / "mqtt_send.csv")
    rx_rows = load_csv(session_dir / "mqtt_rx.csv")
    lora_rows = load_csv(session_dir / "lora.csv")
    plot_rows = load_csv(session_dir / "plot_samples.csv")

    if not plot_rows:
        raise SystemExit("plot_samples.csv is empty — flash a *_plot_capture firmware env first.")
    if not fft_rows:
        raise SystemExit("fft.csv is empty — no FFT updates were captured.")
    if not agg_rows:
        raise SystemExit("agg.csv is empty — no aggregation windows were captured.")
    if not latency_rows:
        raise SystemExit("latency.csv is empty — no MQTT RTT samples were captured.")

    seq, fs_hz, t, y = select_reference_plot_sequence(plot_rows)
    waveform_meta = plot_waveform(plots_dir / "01_waveform_snapshot.png", seq, fs_hz, t, y)
    spectrum_meta = plot_spectrum(plots_dir / "02_fft_spectrum.png", seq, fs_hz, y)
    fs_meta = plot_adaptive_fs(plots_dir / "03_adaptive_fs.png", fft_rows)
    agg_meta = plot_aggregate_delivery(plots_dir / "04_aggregate_mqtt_path.png", agg_rows, send_rows, rx_rows)
    latency_meta = plot_latency_distribution(plots_dir / "05_mqtt_latency_distribution.png", latency_rows)
    lora_meta = plot_lora_latency(plots_dir / "06_lora_latency.png", lora_rows) if lora_rows else {
        "uplink_count": "0",
        "latency_mean_ms": "n/a",
        "latency_min_ms": "n/a",
        "latency_max_ms": "n/a",
    }
    assessments = assess_categories(
        waveform_meta,
        spectrum_meta,
        fft_rows,
        agg_rows,
        send_rows,
        rx_rows,
        latency_rows,
        lora_rows,
    )

    records_path = Path(args.records_path).resolve()
    records_path.write_text(
        build_markdown(
            session_dir,
            plots_dir,
            waveform_meta,
            spectrum_meta,
            fs_meta,
            agg_meta,
            latency_meta,
            lora_meta,
            assessments,
        ),
        encoding="utf-8",
    )

    print(f"[plots] Wrote figures under {plots_dir}")
    print(f"[plots] Updated markdown records at {records_path}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""
Generate lightweight SVG plots for one captured signal family session.

This avoids a matplotlib dependency while still producing repo-friendly plots:
- waveform snapshot from the captured FFT window
- FFT magnitude spectrum of the same window
- adaptive sampling frequency over time
"""

from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path

import numpy as np


WIDTH = 980
HEIGHT = 420
PLOT_LEFT = 70
PLOT_RIGHT = 30
PLOT_TOP = 50
PLOT_BOTTOM = 55


def load_csv(path: Path) -> list[dict[str, str]]:
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


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


def data_to_points(x: np.ndarray, y: np.ndarray, x_min: float, x_max: float, y_min: float, y_max: float) -> str:
    plot_w = WIDTH - PLOT_LEFT - PLOT_RIGHT
    plot_h = HEIGHT - PLOT_TOP - PLOT_BOTTOM

    def scale_x(v: float) -> float:
        if x_max == x_min:
            return PLOT_LEFT
        return PLOT_LEFT + ((v - x_min) / (x_max - x_min)) * plot_w

    def scale_y(v: float) -> float:
        if y_max == y_min:
            return PLOT_TOP + plot_h / 2.0
        return PLOT_TOP + plot_h - ((v - y_min) / (y_max - y_min)) * plot_h

    return " ".join(f"{scale_x(xv):.2f},{scale_y(yv):.2f}" for xv, yv in zip(x, y))


def tick_values(min_val: float, max_val: float, count: int = 5) -> list[float]:
    if math.isclose(min_val, max_val):
        return [min_val]
    return [min_val + (max_val - min_val) * i / (count - 1) for i in range(count)]


def svg_header(title: str, subtitle: str) -> list[str]:
    return [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{WIDTH}" height="{HEIGHT}" viewBox="0 0 {WIDTH} {HEIGHT}">',
        '<rect width="100%" height="100%" fill="#fbfaf6"/>',
        f'<text x="{PLOT_LEFT}" y="28" font-family="Helvetica, Arial, sans-serif" font-size="22" font-weight="700" fill="#17324d">{escape_xml(title)}</text>',
        f'<text x="{PLOT_LEFT}" y="44" font-family="Helvetica, Arial, sans-serif" font-size="12" fill="#5b6c7d">{escape_xml(subtitle)}</text>',
    ]


def escape_xml(text: str) -> str:
    return (
        text.replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace('"', "&quot;")
    )


def draw_axes(lines: list[str], x_ticks: list[float], y_ticks: list[float], x_min: float, x_max: float, y_min: float, y_max: float, x_label: str, y_label: str) -> None:
    plot_w = WIDTH - PLOT_LEFT - PLOT_RIGHT
    plot_h = HEIGHT - PLOT_TOP - PLOT_BOTTOM
    lines.append(f'<rect x="{PLOT_LEFT}" y="{PLOT_TOP}" width="{plot_w}" height="{plot_h}" fill="#fffdfa" stroke="#d9d2c3" stroke-width="1"/>')

    for x_tick in x_ticks:
        x_px = PLOT_LEFT if x_max == x_min else PLOT_LEFT + ((x_tick - x_min) / (x_max - x_min)) * plot_w
        lines.append(f'<line x1="{x_px:.2f}" y1="{PLOT_TOP}" x2="{x_px:.2f}" y2="{PLOT_TOP + plot_h}" stroke="#ece6da" stroke-width="1"/>')
        lines.append(
            f'<text x="{x_px:.2f}" y="{HEIGHT - 22}" text-anchor="middle" font-family="Helvetica, Arial, sans-serif" font-size="11" fill="#58677a">{x_tick:.2f}</text>'
        )

    for y_tick in y_ticks:
        y_px = PLOT_TOP + plot_h / 2.0 if y_max == y_min else PLOT_TOP + plot_h - ((y_tick - y_min) / (y_max - y_min)) * plot_h
        lines.append(f'<line x1="{PLOT_LEFT}" y1="{y_px:.2f}" x2="{PLOT_LEFT + plot_w}" y2="{y_px:.2f}" stroke="#ece6da" stroke-width="1"/>')
        lines.append(
            f'<text x="{PLOT_LEFT - 10}" y="{y_px + 4:.2f}" text-anchor="end" font-family="Helvetica, Arial, sans-serif" font-size="11" fill="#58677a">{y_tick:.2f}</text>'
        )

    lines.append(
        f'<text x="{PLOT_LEFT + plot_w / 2:.2f}" y="{HEIGHT - 6}" text-anchor="middle" font-family="Helvetica, Arial, sans-serif" font-size="12" fill="#35495e">{escape_xml(x_label)}</text>'
    )
    lines.append(
        f'<text x="20" y="{PLOT_TOP + plot_h / 2:.2f}" text-anchor="middle" font-family="Helvetica, Arial, sans-serif" font-size="12" fill="#35495e" transform="rotate(-90 20 {PLOT_TOP + plot_h / 2:.2f})">{escape_xml(y_label)}</text>'
    )


def save_svg(path: Path, lines: list[str]) -> None:
    lines.append("</svg>")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def plot_waveform(out_path: Path, signal_name: str, seq: int, fs_hz: float, t: np.ndarray, y: np.ndarray) -> dict[str, str]:
    y_pad = max(0.2, 0.08 * max(1.0, float(np.max(np.abs(y)))))
    x_min, x_max = float(np.min(t)), float(np.max(t))
    y_min, y_max = float(np.min(y) - y_pad), float(np.max(y) + y_pad)

    lines = svg_header(
        f"{signal_name} waveform snapshot",
        f"Raw DUT FFT window, seq {seq}, fs={fs_hz:.2f} Hz, n={len(y)}",
    )
    draw_axes(lines, tick_values(x_min, x_max), tick_values(y_min, y_max), x_min, x_max, y_min, y_max, "Time (s)", "Signal value")
    points = data_to_points(t, y, x_min, x_max, y_min, y_max)
    lines.append(f'<polyline fill="none" stroke="#0b7189" stroke-width="2" points="{points}"/>')
    save_svg(out_path, lines)
    return {
        "seq": str(seq),
        "fs_hz": f"{fs_hz:.3f}",
        "samples": str(len(y)),
        "y_min": f"{float(np.min(y)):+.4f}",
        "y_max": f"{float(np.max(y)):+.4f}",
    }


def plot_spectrum(out_path: Path, signal_name: str, seq: int, fs_hz: float, y: np.ndarray) -> dict[str, str]:
    window = np.hamming(len(y))
    spectrum = np.fft.rfft(y * window)
    freqs = np.fft.rfftfreq(len(y), d=1.0 / fs_hz)
    mags = np.abs(spectrum)

    x_min, x_max = 0.0, float(np.max(freqs))
    y_min, y_max = 0.0, float(np.max(mags) * 1.08 if len(mags) else 1.0)

    lines = svg_header(
        f"{signal_name} FFT magnitude spectrum",
        f"Computed from DUT raw window, seq {seq}, fs={fs_hz:.2f} Hz",
    )
    draw_axes(lines, tick_values(x_min, x_max), tick_values(y_min, y_max), x_min, x_max, y_min, y_max, "Frequency (Hz)", "Magnitude")
    points = data_to_points(freqs, mags, x_min, x_max, y_min, y_max)
    lines.append(f'<polyline fill="none" stroke="#d97a1d" stroke-width="2" points="{points}"/>')

    peak_bins = np.argsort(mags[1:])[-3:] + 1 if len(mags) > 3 else np.arange(1, len(mags))
    peak_bins = peak_bins[np.argsort(mags[peak_bins])[::-1]]
    plot_w = WIDTH - PLOT_LEFT - PLOT_RIGHT
    plot_h = HEIGHT - PLOT_TOP - PLOT_BOTTOM
    peak_labels: list[str] = []
    for idx in peak_bins[:3]:
        f_hz = float(freqs[idx])
        mag = float(mags[idx])
        x_px = PLOT_LEFT + ((f_hz - x_min) / (x_max - x_min)) * plot_w if x_max > x_min else PLOT_LEFT
        y_px = PLOT_TOP + plot_h - ((mag - y_min) / (y_max - y_min)) * plot_h if y_max > y_min else PLOT_TOP + plot_h / 2.0
        lines.append(f'<circle cx="{x_px:.2f}" cy="{y_px:.2f}" r="4.5" fill="#a12a2a"/>')
        lines.append(
            f'<text x="{x_px + 6:.2f}" y="{y_px - 8:.2f}" font-family="Helvetica, Arial, sans-serif" font-size="11" fill="#7a1c1c">{f_hz:.2f} Hz</text>'
        )
        peak_labels.append(f"{f_hz:.2f}")

    save_svg(out_path, lines)
    return {
        "seq": str(seq),
        "fs_hz": f"{fs_hz:.3f}",
        "peak_1_hz": peak_labels[0] if peak_labels else "n/a",
        "peak_2_hz": peak_labels[1] if len(peak_labels) > 1 else "n/a",
        "peak_3_hz": peak_labels[2] if len(peak_labels) > 2 else "n/a",
    }


def plot_adaptive_fs(out_path: Path, signal_name: str, fft_rows: list[dict[str, str]]) -> dict[str, str]:
    idx = np.arange(1, len(fft_rows) + 1, dtype=float)
    dominant = np.array([float(row["dominant_hz"]) for row in fft_rows], dtype=float)
    fs_vals = np.array([float(row["fs_hz"]) for row in fft_rows], dtype=float)
    policy_target = np.clip(np.round((dominant * 8.0) / 5.0) * 5.0, 20.0, 50.0)

    x_min, x_max = 1.0, float(max(2, len(idx)))
    y_min = 0.0
    y_max = float(max(np.max(fs_vals), np.max(policy_target)) * 1.15)

    lines = svg_header(
        f"{signal_name} adaptive sampling history",
        f"{len(fft_rows)} FFT updates captured from the DUT",
    )
    draw_axes(lines, tick_values(x_min, x_max), tick_values(y_min, y_max), x_min, x_max, y_min, y_max, "FFT update index", "Frequency (Hz)")

    duty_line = data_to_points(idx, fs_vals, x_min, x_max, y_min, y_max)
    target_line = data_to_points(idx, policy_target, x_min, x_max, y_min, y_max)
    lines.append(f'<polyline fill="none" stroke="#155e63" stroke-width="2.5" points="{duty_line}"/>')
    lines.append(f'<polyline fill="none" stroke="#ef7d00" stroke-width="2" stroke-dasharray="8 6" points="{target_line}"/>')

    plot_w = WIDTH - PLOT_LEFT - PLOT_RIGHT
    plot_h = HEIGHT - PLOT_TOP - PLOT_BOTTOM
    y_target = 40.0
    if y_max > y_min:
        y_px = PLOT_TOP + plot_h - ((y_target - y_min) / (y_max - y_min)) * plot_h
        lines.append(f'<line x1="{PLOT_LEFT}" y1="{y_px:.2f}" x2="{PLOT_LEFT + plot_w}" y2="{y_px:.2f}" stroke="#7b8794" stroke-dasharray="4 6" stroke-width="1.5"/>')
        lines.append(
            f'<text x="{PLOT_LEFT + plot_w - 6}" y="{y_px - 6:.2f}" text-anchor="end" font-family="Helvetica, Arial, sans-serif" font-size="11" fill="#58677a">40 Hz target</text>'
        )

    lines.append(f'<rect x="{WIDTH - 230}" y="{PLOT_TOP + 8}" width="180" height="52" rx="8" fill="#fffdfa" stroke="#d9d2c3"/>')
    lines.append(f'<line x1="{WIDTH - 214}" y1="{PLOT_TOP + 24}" x2="{WIDTH - 174}" y2="{PLOT_TOP + 24}" stroke="#155e63" stroke-width="2.5"/>')
    lines.append(f'<text x="{WIDTH - 166}" y="{PLOT_TOP + 28}" font-family="Helvetica, Arial, sans-serif" font-size="11" fill="#35495e">adaptive fs</text>')
    lines.append(f'<line x1="{WIDTH - 214}" y1="{PLOT_TOP + 44}" x2="{WIDTH - 174}" y2="{PLOT_TOP + 44}" stroke="#ef7d00" stroke-width="2" stroke-dasharray="8 6"/>')
    lines.append(f'<text x="{WIDTH - 166}" y="{PLOT_TOP + 48}" font-family="Helvetica, Arial, sans-serif" font-size="11" fill="#35495e">8x policy</text>')
    save_svg(out_path, lines)
    return {
        "fft_updates": str(len(fft_rows)),
        "dominant_mean_hz": f"{float(np.mean(dominant)):.3f}",
        "fs_mean_hz": f"{float(np.mean(fs_vals)):.3f}",
        "fs_min_hz": f"{float(np.min(fs_vals)):.3f}",
        "fs_max_hz": f"{float(np.max(fs_vals)):.3f}",
    }


def write_summary(path: Path, signal_name: str, waveform_meta: dict[str, str], spectrum_meta: dict[str, str], adaptive_meta: dict[str, str]) -> None:
    lines = [
        f"# {signal_name} Signal Plot Summary",
        "",
        f"- Waveform window: seq `{waveform_meta['seq']}`, `fs={waveform_meta['fs_hz']} Hz`, `n={waveform_meta['samples']}`",
        f"- Signal amplitude range: `{waveform_meta['y_min']}` to `{waveform_meta['y_max']}`",
        f"- Dominant spectral peaks: `{spectrum_meta['peak_1_hz']} Hz`, `{spectrum_meta['peak_2_hz']} Hz`, `{spectrum_meta['peak_3_hz']} Hz`",
        f"- Adaptive fs mean/min/max: `{adaptive_meta['fs_mean_hz']} / {adaptive_meta['fs_min_hz']} / {adaptive_meta['fs_max_hz']} Hz`",
        f"- Dominant frequency mean: `{adaptive_meta['dominant_mean_hz']} Hz` across `{adaptive_meta['fft_updates']}` FFT updates",
        "",
        "Generated by `tools/generate_signal_family_plots.py` from the raw DUT capture CSVs.",
    ]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate lightweight SVG plots for one captured signal-family session")
    parser.add_argument("session_dir", help="Session directory produced by tools/plot_capture.py")
    parser.add_argument("--signal-name", required=True, help="Friendly label, e.g. clean, noise, spikes")
    parser.add_argument("--output-dir", default="", help="Optional output directory; defaults to <session_dir>/signal_plots")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    session_dir = Path(args.session_dir).resolve()
    output_dir = Path(args.output_dir).resolve() if args.output_dir else session_dir / "signal_plots"
    ensure_dir(output_dir)

    plot_rows = load_csv(session_dir / "plot_samples.csv")
    fft_rows = load_csv(session_dir / "fft.csv")
    if not fft_rows:
        raise SystemExit("fft.csv is empty")

    seq, fs_hz, t, y = select_reference_plot_sequence(plot_rows)
    signal_name = args.signal_name.strip()

    waveform_meta = plot_waveform(output_dir / f"{signal_name}_waveform.svg", signal_name, seq, fs_hz, t, y)
    spectrum_meta = plot_spectrum(output_dir / f"{signal_name}_spectrum.svg", signal_name, seq, fs_hz, y)
    adaptive_meta = plot_adaptive_fs(output_dir / f"{signal_name}_adaptive_fs.svg", signal_name, fft_rows)
    write_summary(output_dir / f"{signal_name}_summary.md", signal_name, waveform_meta, spectrum_meta, adaptive_meta)

    print(f"[signal-plots] wrote plots to {output_dir}")


if __name__ == "__main__":
    main()

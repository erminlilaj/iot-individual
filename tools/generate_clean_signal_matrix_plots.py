#!/usr/bin/env python3
"""
Generate lightweight waveform and spectrum plots for the clean-signal matrix
used in the README bonus section.

This reuses the existing SVG helpers so the bonus visuals stay reproducible and
do not depend on matplotlib. The plots are formula-based reference visuals,
while the dominant/adaptive numbers in the summaries reflect the current
8x/20-50 Hz/5 Hz-step adaptive policy implemented in firmware.
"""

from __future__ import annotations

from pathlib import Path

import numpy as np

from generate_signal_family_plots import (
    ensure_dir,
    plot_spectrum,
    plot_waveform,
)


BASE_FS_HZ = 100.0
WINDOW_SAMPLES = 256


SIGNALS = [
    {
        "id": "clean_a",
        "title": "Signal A",
        "formula": "2*sin(2*pi*3*t) + 4*sin(2*pi*5*t)",
        "expected_highest_hz": 5.0,
        "observed_dominant_hz": 5.02,
        "adaptive_fs_hz": 40.00,
        "window_n": 200,
        "session_dir": "tools/plot_sessions/20260513_090512_clean_40hz_new-fixes_retry",
        "note": "reference case where the dominant and highest tone are the same, so the current 8x policy adapts to 40 Hz.",
        "signal_fn": lambda t: 2.0 * np.sin(2.0 * np.pi * 3.0 * t) + 4.0 * np.sin(2.0 * np.pi * 5.0 * t),
    },
    {
        "id": "clean_b",
        "title": "Signal B",
        "formula": "4*sin(2*pi*3*t) + 2*sin(2*pi*9*t)",
        "expected_highest_hz": 9.0,
        "observed_dominant_hz": 3.01,
        "adaptive_fs_hz": 25.00,
        "window_n": 125,
        "session_dir": "tools/plot_sessions/20260424_104417_clean_b_dut",
        "note": "the 9 Hz tone exists, but the 3 Hz tone is stronger, so the controller follows the dominant peak and the current 8x policy targets about 25 Hz.",
        "signal_fn": lambda t: 4.0 * np.sin(2.0 * np.pi * 3.0 * t) + 2.0 * np.sin(2.0 * np.pi * 9.0 * t),
    },
    {
        "id": "clean_c",
        "title": "Signal C",
        "formula": "2*sin(2*pi*2*t) + 3*sin(2*pi*5*t) + 1.5*sin(2*pi*7*t)",
        "expected_highest_hz": 7.0,
        "observed_dominant_hz": 5.03,
        "adaptive_fs_hz": 40.00,
        "window_n": 200,
        "session_dir": "tools/plot_sessions/20260424_104628_clean_c_dut",
        "note": "with three tones present, the 5 Hz component still dominates, so the current 8x policy targets 40 Hz.",
        "signal_fn": lambda t: 2.0 * np.sin(2.0 * np.pi * 2.0 * t) + 3.0 * np.sin(2.0 * np.pi * 5.0 * t) + 1.5 * np.sin(2.0 * np.pi * 7.0 * t),
    },
]


def write_signal_summary(path: Path, signal: dict[str, object], waveform_meta: dict[str, str], spectrum_meta: dict[str, str]) -> None:
    lines = [
        f"# {signal['title']}",
        "",
        f"- Formula: `{signal['formula']}`",
        f"- Expected highest frequency: `{signal['expected_highest_hz']:.2f} Hz`",
        f"- Observed dominant frequency: `{signal['observed_dominant_hz']:.2f} Hz`",
        f"- Adaptive sampling rate: `{signal['adaptive_fs_hz']:.2f} Hz`",
        f"- 5 s window sample count at adaptive rate: `{signal['window_n']}`",
        f"- Reference waveform window: seq `{waveform_meta['seq']}`, `fs={waveform_meta['fs_hz']} Hz`, `n={waveform_meta['samples']}`",
        f"- Main spectrum peaks in the plotted window: `{spectrum_meta['peak_1_hz']} Hz`, `{spectrum_meta['peak_2_hz']} Hz`, `{spectrum_meta['peak_3_hz']} Hz`",
        f"- Interpretation: {signal['note']}",
        "",
        "Waveform and spectrum plots are generated from the implemented signal formula. The dominant frequency values are from the captured DUT/reference sessions, and the adaptive sampling rate is recomputed using the current firmware policy.",
    ]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    output_root = Path("source/results/20260424_clean_signal_matrix_plots").resolve()
    ensure_dir(output_root)

    summary_lines = [
        "# Clean Signal Matrix Plot Bundle",
        "",
        "These plots are derived from the implemented signal formulas and current adaptive policy used in the README bonus section.",
        "",
    ]

    for signal in SIGNALS:
        t_seconds = np.arange(WINDOW_SAMPLES, dtype=float) / BASE_FS_HZ
        values = signal["signal_fn"](t_seconds)
        seq = 1
        fs_hz = BASE_FS_HZ

        signal_dir = output_root / str(signal["id"])
        ensure_dir(signal_dir)

        waveform_meta = plot_waveform(signal_dir / f"{signal['id']}_waveform.svg", str(signal["id"]), seq, fs_hz, t_seconds, values)
        spectrum_meta = plot_spectrum(signal_dir / f"{signal['id']}_spectrum.svg", str(signal["id"]), seq, fs_hz, values)

        write_signal_summary(signal_dir / "SUMMARY.md", signal, waveform_meta, spectrum_meta)

        summary_lines.extend(
            [
                f"## {signal['title']}",
                "",
                f"- Session: `{signal['session_dir']}`",
                f"- Spectrum plot: `{signal['id']}/{signal['id']}_spectrum.svg`",
                f"- Waveform plot: `{signal['id']}/{signal['id']}_waveform.svg`",
                f"- Summary: `{signal['id']}/SUMMARY.md`",
                "",
            ]
        )

    (output_root / "SUMMARY.md").write_text("\n".join(summary_lines) + "\n", encoding="utf-8")
    print(f"[clean-signal-matrix-plots] wrote bundle to {output_root}")


if __name__ == "__main__":
    main()

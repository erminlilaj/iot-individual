#!/usr/bin/env python3
"""
Parse bonus-oriented serial logs and emit compact CSV summaries.

This script is intentionally stdlib-only so it can run on a fresh checkout
without extra dependencies beyond the existing Python runtime.
"""

from __future__ import annotations

import argparse
import csv
import re
from pathlib import Path

RE_CONFIG = re.compile(
    r"\[BONUS-CONFIG\]\s+mode=(\w+)\s+noise_sigma=([\d.]+)\s+spike_pct=(\d+)\s+"
    r"spike_dist=([^\s]+)\s+baseline_fs=([\d.]+)\s+fft_n=(\d+)"
)
RE_SIGNAL = re.compile(
    r"\[BONUS-SIGNAL\]\s+mode=(\w+)\s+spike_pct=(\d+)\s+dominant=([\d.]+)\s+"
    r"adaptive_fs=([\d.]+)\s+baseline_fs=([\d.]+)\s+window_n=(\d+)\s+baseline_window_n=(\d+)\s+"
    r"duty_pct=([\d.]+)\s+i_avg_ma=([\d.]+)\s+savings_pct=([-\d.]+)"
)
RE_FILTER = re.compile(
    r"\[BONUS-FILTER\]\s+filter=(\w+)\s+k=(\d+)\s+fs=([\d.]+)\s+tpr=([\d.]+)\s+"
    r"fpr=([\d.]+)\s+mer=([-\d.]+)\s+exec_us=([\d.]+)\s+ram_b=(\d+)\s+added_delay_ms=([\d.]+)"
)
RE_FFT = re.compile(
    r"\[BONUS-FFT\]\s+seq=(\d+)\s+raw_peak=([\d.]+)\s+zscore_peak=([\d.]+)\s+"
    r"hampel5_peak=([\d.]+)\s+hampel20_peak=([\d.]+)\s+hampel50_peak=([\d.]+)\s+"
    r"raw_fs=([\d.]+)\s+zscore_fs=([\d.]+)\s+hampel5_fs=([\d.]+)\s+hampel20_fs=([\d.]+)\s+hampel50_fs=([\d.]+)"
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("logs", nargs="+", help="Serial log paths to parse")
    parser.add_argument(
        "--output-dir",
        default="tools/bonus_results",
        help="Directory where the CSV summaries should be written",
    )
    return parser.parse_args()


def run_id_for(path: Path) -> str:
    parent = path.parent.name
    return parent if parent else path.stem


def stable_label(mode: str, dominant_hz: float, adaptive_fs_hz: float) -> str:
    if mode == "clean":
        return "yes" if abs(dominant_hz - 5.0) <= 0.2 and 9.5 <= adaptive_fs_hz <= 10.5 else "no"
    if mode == "noise":
        return "yes" if abs(dominant_hz - 5.0) <= 0.5 and 9.0 <= adaptive_fs_hz <= 11.0 else "no"
    return "yes" if 8.0 <= adaptive_fs_hz <= 12.0 else "mixed"


def quality_note_for_k(k: int) -> str:
    if k == 5:
        return "fastest_and_lightest"
    if k == 20:
        return "balanced_candidate"
    if k == 50:
        return "heaviest_best_statistics"
    return "custom"


def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def write_csv(path: Path, header: list[str], rows: list[dict[str, object]]) -> None:
    with open(path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=header)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def main() -> int:
    args = parse_args()
    output_dir = Path(args.output_dir)
    ensure_dir(output_dir)

    signal_rows: list[dict[str, object]] = []
    filter_rows: list[dict[str, object]] = []
    fft_rows: list[dict[str, object]] = []
    tradeoff_rows: list[dict[str, object]] = []

    for raw_path in args.logs:
        path = Path(raw_path)
        if not path.exists():
            raise FileNotFoundError(path)

        run_id = run_id_for(path)
        config: dict[str, object] = {}
        last_signal: dict[str, object] | None = None
        last_filters: dict[tuple[str, int], dict[str, object]] = {}
        last_fft: dict[str, object] | None = None

        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            if m := RE_CONFIG.search(line):
                config = {
                    "mode": m.group(1),
                    "noise_sigma": float(m.group(2)),
                    "spike_pct": int(m.group(3)),
                    "spike_dist": m.group(4),
                    "baseline_fs_hz": float(m.group(5)),
                    "fft_n": int(m.group(6)),
                }
                continue

            if m := RE_SIGNAL.search(line):
                last_signal = {
                    "run_id": run_id,
                    "mode": m.group(1),
                    "spike_pct": int(m.group(2)),
                    "dominant_hz": float(m.group(3)),
                    "adaptive_fs_hz": float(m.group(4)),
                    "baseline_fs_hz": float(m.group(5)),
                    "adaptive_window_n": int(m.group(6)),
                    "baseline_window_n": int(m.group(7)),
                    "duty_pct": float(m.group(8)),
                    "i_avg_proxy_ma": float(m.group(9)),
                    "savings_pct": float(m.group(10)),
                }
                continue

            if m := RE_FILTER.search(line):
                row = {
                    "run_id": run_id,
                    "mode": config.get("mode", ""),
                    "p_spike": int(config.get("spike_pct", 0)),
                    "filter": m.group(1),
                    "k": int(m.group(2)),
                    "fs_hz": float(m.group(3)),
                    "tpr": float(m.group(4)),
                    "fpr": float(m.group(5)),
                    "mer": float(m.group(6)),
                    "exec_us_avg": float(m.group(7)),
                    "ram_b": int(m.group(8)),
                    "added_delay_ms": float(m.group(9)),
                }
                last_filters[(row["filter"], row["k"])] = row
                continue

            if m := RE_FFT.search(line):
                last_fft = {
                    "run_id": run_id,
                    "mode": config.get("mode", ""),
                    "p_spike": int(config.get("spike_pct", 0)),
                    "seq": int(m.group(1)),
                    "raw_peak_hz": float(m.group(2)),
                    "zscore_peak_hz": float(m.group(3)),
                    "hampel5_peak_hz": float(m.group(4)),
                    "hampel20_peak_hz": float(m.group(5)),
                    "hampel50_peak_hz": float(m.group(6)),
                    "raw_fs_hz": float(m.group(7)),
                    "zscore_fs_hz": float(m.group(8)),
                    "hampel5_fs_hz": float(m.group(9)),
                    "hampel20_fs_hz": float(m.group(10)),
                    "hampel50_fs_hz": float(m.group(11)),
                }

        if last_signal:
            last_signal["noise_sigma"] = float(config.get("noise_sigma", 0.0))
            last_signal["spike_dist"] = config.get("spike_dist", "")
            last_signal["fft_n"] = int(config.get("fft_n", 0))
            last_signal["stable_yes_no"] = stable_label(
                str(last_signal["mode"]),
                float(last_signal["dominant_hz"]),
                float(last_signal["adaptive_fs_hz"]),
            )
            signal_rows.append(last_signal)

        for row in sorted(last_filters.values(), key=lambda item: (item["filter"], item["k"])):
            filter_rows.append(row)
            if row["filter"] == "hampel":
                tradeoff_rows.append({
                    "run_id": row["run_id"],
                    "p_spike": row["p_spike"],
                    "window_setting": f"k={row['k']}",
                    "ram_bytes": row["ram_b"],
                    "exec_us_avg": row["exec_us_avg"],
                    "added_delay_ms": row["added_delay_ms"],
                    "quality_notes": quality_note_for_k(int(row["k"])),
                })

        if last_fft:
            fft_rows.append(last_fft)

    signal_rows.sort(key=lambda row: (str(row["mode"]), int(row["spike_pct"]), str(row["run_id"])))
    filter_rows.sort(key=lambda row: (int(row["p_spike"]), str(row["filter"]), int(row["k"]), str(row["run_id"])))
    fft_rows.sort(key=lambda row: (int(row["p_spike"]), str(row["run_id"])))
    tradeoff_rows.sort(key=lambda row: (int(row["p_spike"]), str(row["window_setting"]), str(row["run_id"])))

    write_csv(
        output_dir / "bonus_signal_matrix.csv",
        [
            "run_id",
            "mode",
            "spike_pct",
            "noise_sigma",
            "spike_dist",
            "fft_n",
            "dominant_hz",
            "adaptive_fs_hz",
            "baseline_fs_hz",
            "adaptive_window_n",
            "baseline_window_n",
            "duty_pct",
            "i_avg_proxy_ma",
            "savings_pct",
            "stable_yes_no",
        ],
        signal_rows,
    )
    write_csv(
        output_dir / "bonus_filter_matrix.csv",
        [
            "run_id",
            "mode",
            "p_spike",
            "filter",
            "k",
            "fs_hz",
            "tpr",
            "fpr",
            "mer",
            "exec_us_avg",
            "ram_b",
            "added_delay_ms",
        ],
        filter_rows,
    )
    write_csv(
        output_dir / "bonus_fft_contamination.csv",
        [
            "run_id",
            "mode",
            "p_spike",
            "seq",
            "raw_peak_hz",
            "zscore_peak_hz",
            "hampel5_peak_hz",
            "hampel20_peak_hz",
            "hampel50_peak_hz",
            "raw_fs_hz",
            "zscore_fs_hz",
            "hampel5_fs_hz",
            "hampel20_fs_hz",
            "hampel50_fs_hz",
        ],
        fft_rows,
    )
    write_csv(
        output_dir / "bonus_window_tradeoff.csv",
        [
            "run_id",
            "p_spike",
            "window_setting",
            "ram_bytes",
            "exec_us_avg",
            "added_delay_ms",
            "quality_notes",
        ],
        tradeoff_rows,
    )

    print(f"[bonus-extract] wrote summaries to {output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""
power_bridge.py

Reads the monitor ESP32 serial stream, filters CSV lines, strips the
'CSV,timestamp,' prefix and forwards clean comma-separated floats to a
virtual serial port that BetterSerialPlotter can open.

Channels forwarded (5):
  [0] bus_V  [1] shunt_mV  [2] load_V  [3] current_mA  [4] power_mW

Setup (run steps in order):
  1. Open a terminal and create a virtual serial pair:
       socat -d -d pty,raw,echo=0 pty,raw,echo=0
     It prints two paths like /dev/pts/3 and /dev/pts/4. Leave it running.

  2. Run this bridge (use ONE of the two pts paths as --out):
       python3 tools/power_bridge.py --out /dev/pts/3

  3. In BetterSerialPlotter select the OTHER pts path (/dev/pts/4) at 115200.
"""

import argparse
import serial
import sys
import time


def parse_args():
    ap = argparse.ArgumentParser(description="CSV bridge for BetterSerialPlotter")
    ap.add_argument("--in",   dest="port_in",  default="/dev/ttyUSB0",
                    help="monitor ESP32 serial port (default: /dev/ttyUSB0)")
    ap.add_argument("--out",  dest="port_out",  required=True,
                    help="virtual serial port from socat (e.g. /dev/pts/3)")
    ap.add_argument("--baud", type=int, default=115200)
    return ap.parse_args()


def main():
    args = parse_args()

    # Open monitor serial port without toggling reset lines
    ser_in = serial.Serial()
    ser_in.port     = args.port_in
    ser_in.baudrate = args.baud
    ser_in.timeout  = 1
    ser_in.dsrdtr   = False
    ser_in.rtscts   = False
    ser_in.open()
    ser_in.setDTR(False)
    ser_in.setRTS(False)

    ser_out = serial.Serial(args.port_out, args.baud, timeout=1)

    print(f"[bridge] {args.port_in} → {args.port_out}  |  open BetterSerialPlotter on the OTHER socat port")
    print("[bridge] Channels: bus_V | shunt_mV | load_V | current_mA | power_mW")

    lines_in  = 0
    lines_out = 0

    try:
        while True:
            raw = ser_in.readline()
            if not raw:
                continue

            line = raw.decode("utf-8", errors="replace").strip()
            lines_in += 1

            if not line.startswith("CSV,"):
                continue

            # Format: CSV,<timestamp_ms>,<bus_V>,<shunt_mV>,<load_V>,<current_mA>,<power_mW>
            parts = line.split(",")
            if len(parts) < 7:
                continue

            # Drop "CSV" (index 0) and timestamp (index 1); forward the 5 data columns
            data_cols = parts[2:7]

            # Validate all columns are numeric before forwarding
            try:
                [float(v) for v in data_cols]
            except ValueError:
                continue

            out_line = ",".join(data_cols) + "\n"
            ser_out.write(out_line.encode())
            ser_out.flush()

            lines_out += 1
            if lines_out % 25 == 0:
                print(f"[bridge] forwarded {lines_out} samples  last: {out_line.strip()}")

    except KeyboardInterrupt:
        print(f"\n[bridge] stopped — forwarded {lines_out}/{lines_in} lines")
    finally:
        ser_in.close()
        ser_out.close()


if __name__ == "__main__":
    main()

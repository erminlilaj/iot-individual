#!/usr/bin/env python3
"""
MQTT edge server for the IoT adaptive sampling pipeline.

Subscribes to:
  eri/iot/average  — receives the 5-second window mean from the ESP32
  eri/iot/ping     — echoes the payload back on eri/iot/pong for RTT measurement

Logs all received averages and RTT measurements to edge_log.csv.

Usage:
    # 1. Start a local Mosquitto broker (install once: sudo apt install mosquitto)
    mosquitto -v

    # 2. Run this server (in a separate terminal)
    python tools/edge_server.py

    # 3. Optionally sniff all topics
    mosquitto_sub -t 'eri/iot/#' -v
"""

import csv
import os
import queue
import sys
import threading
import time

import paho.mqtt.client as mqtt

BROKER_HOST = os.getenv("MQTT_BROKER_HOST", "127.0.0.1")
BROKER_PORT = int(os.getenv("MQTT_BROKER_PORT", "1883"))

TOPIC_AVG  = "eri/iot/average"
TOPIC_AGG  = "eri/iot/aggregate"
TOPIC_PING = "eri/iot/ping"
TOPIC_PONG = "eri/iot/pong"

LOG_PATH = os.path.join(os.path.dirname(__file__), "edge_log.csv")

# Ping timestamps keyed by payload for RTT tracking
_ping_times: dict[str, float] = {}

# Pong payloads to publish — deferred out of the on_message callback
# to avoid re-entering the paho network loop from within a callback.
_pong_queue: queue.SimpleQueue[str] = queue.SimpleQueue()


def _open_csv():
    exists = os.path.exists(LOG_PATH)
    f = open(LOG_PATH, "a", newline="", buffering=1)
    w = csv.writer(f)
    if not exists:
        w.writerow(["iso_timestamp", "topic", "value"])
    return w, f


def on_connect(client, userdata, flags, reason_code, properties):
    if reason_code.is_failure:
        print(f"[SERVER] Connection refused — {reason_code}", file=sys.stderr)
        print("         Is mosquitto running? Try: mosquitto -v", file=sys.stderr)
        return
    print(f"[SERVER] Connected to broker at {BROKER_HOST}:{BROKER_PORT}")
    client.subscribe(TOPIC_AVG)
    client.subscribe(TOPIC_AGG)
    client.subscribe(TOPIC_PING)
    print(f"[SERVER] Subscribed to {TOPIC_AVG}, {TOPIC_AGG}, and {TOPIC_PING}")
    print(f"[SERVER] Logging to {LOG_PATH}")
    print("[SERVER] Waiting for messages from ESP32...\n")


def on_message(client, userdata, msg):
    csv_writer = userdata["writer"]
    topic   = msg.topic
    payload = msg.payload.decode("utf-8", errors="replace").strip()
    ts      = time.strftime("%H:%M:%S")
    iso     = time.strftime("%Y-%m-%dT%H:%M:%S")

    if topic == TOPIC_AVG:
        try:
            val = float(payload)
            print(f"[{ts}] [EDGE] average received: {val:+.4f}")
            csv_writer.writerow([iso, "average", f"{val:.4f}"])
        except ValueError:
            print(f"[{ts}] [EDGE] average (raw): {payload}")

    elif topic == TOPIC_AGG:
        print(f"[{ts}] [EDGE] aggregate received: {payload}")
        csv_writer.writerow([iso, "aggregate", payload])

    elif topic == TOPIC_PING:
        _ping_times[payload] = time.monotonic()
        _pong_queue.put(payload)  # publish from sender thread, not callback

    elif topic == TOPIC_PONG:
        sent = _ping_times.pop(payload, None)
        if sent is not None:
            rtt_ms = (time.monotonic() - sent) * 1000
            csv_writer.writerow([iso, "rtt_ms", f"{rtt_ms:.1f}"])


def on_disconnect(client, userdata, flags, reason_code, properties):
    if reason_code.is_failure:
        print(f"[SERVER] Unexpected disconnect ({reason_code}) — will auto-reconnect")


def _pong_sender(client: mqtt.Client, stop: threading.Event) -> None:
    while not stop.is_set():
        try:
            payload = _pong_queue.get(timeout=0.1)
            client.publish(TOPIC_PONG, payload)
        except queue.Empty:
            pass


def main():
    writer, log_file = _open_csv()

    client = mqtt.Client(
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
        client_id="eri-edge-server",
        clean_session=True,
    )
    client.user_data_set({"writer": writer})
    client.on_connect    = on_connect
    client.on_message    = on_message
    client.on_disconnect = on_disconnect
    client.reconnect_delay_set(min_delay=2, max_delay=30)

    try:
        client.connect(BROKER_HOST, BROKER_PORT, keepalive=60)
    except ConnectionRefusedError:
        print(f"[SERVER] Cannot connect to {BROKER_HOST}:{BROKER_PORT}", file=sys.stderr)
        print("         Start the broker first: mosquitto -v", file=sys.stderr)
        log_file.close()
        sys.exit(1)

    print(f"[SERVER] Connecting to {BROKER_HOST}:{BROKER_PORT} ...")

    stop_event = threading.Event()
    sender = threading.Thread(target=_pong_sender, args=(client, stop_event), daemon=True)
    sender.start()

    try:
        client.loop_forever()
    finally:
        stop_event.set()
        sender.join(timeout=2)
        log_file.close()


if __name__ == "__main__":
    main()

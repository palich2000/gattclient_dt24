#!/usr/bin/env python3
"""Finite real BLE -> MQTT probe via SSH to a LOCAL isolated broker.

Requires a staged/built receiver on the explicitly supplied host. No service is
installed and no home-broker topics are touched. Run from the repository root.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import socket
import subprocess
import time


def run(*args, **kwargs):
    return subprocess.run(*args, check=True, timeout=20, **kwargs)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", required=True)
    parser.add_argument("--remote-dir", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--duration", type=int, default=70)
    parser.add_argument("--remote-port", type=int, default=18884)
    args = parser.parse_args()
    if not 10 <= args.duration <= 600:
        parser.error("duration must be 10..600")
    if not 1024 <= args.remote_port <= 65535:
        parser.error("remote-port must be 1024..65535")
    out = Path(args.output).resolve()
    out.mkdir(parents=True, exist_ok=False)
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        port = s.getsockname()[1]
    broker_cfg = out / "broker.conf"
    broker_cfg.write_text(f"listener {port} 127.0.0.1\nallow_anonymous true\npersistence false\n")
    config = out / "probe.local.ini"
    config.write_text(f"[bluetooth]\nadapter=hci0\nmac=DE:4D:F8:3F:DA:91\n[mqtt]\nenabled=true\nhost=127.0.0.1\nport={args.remote_port}\nclient_id=ruuvi-live-probe\ntopic_prefix=test/ruuvi-live\n[app]\npublish_interval=5\nstale_timeout=30\n")
    config.chmod(0o600)
    ssh = ["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=10"]
    remote_cfg = args.remote_dir + "/config/probe.local.ini"
    run(["scp", "-q", "-o", "BatchMode=yes", str(config), args.host + ":" + remote_cfg])
    checksum = run(ssh + [args.host, "sha256sum " + shlex.quote(remote_cfg)], capture_output=True, text=True).stdout.split()[0]
    assert checksum == hashlib.sha256(config.read_bytes()).hexdigest(), "Remote config mismatch"
    broker = sub = None
    try:
        with (out / "broker.log").open("w") as log:
            broker = subprocess.Popen(["mosquitto", "-c", str(broker_cfg)], stdout=log, stderr=log)
        for _ in range(50):
            try:
                with socket.create_connection(("127.0.0.1", port), timeout=0.1):
                    break
            except OSError:
                time.sleep(0.1)
        else:
            raise RuntimeError("Local broker did not start")
        with (out / "mqtt.log").open("w") as capture:
            sub = subprocess.Popen(["mosquitto_sub", "-h", "127.0.0.1", "-p", str(port), "-t", "test/ruuvi-live/#", "-v"], stdout=capture)
        time.sleep(0.3)
        command = "cd " + shlex.quote(args.remote_dir) + " && timeout --signal=TERM --kill-after=5 " + str(args.duration + 15) + " ./src/ruuvi-mqtt -C config/probe.local.ini --duration " + str(args.duration)
        with (out / "console.jsonl").open("w") as stdout, (out / "receiver.log").open("w") as stderr:
            subprocess.run(ssh + ["-o", "ExitOnForwardFailure=yes", "-R", f"127.0.0.1:{args.remote_port}:127.0.0.1:{port}", args.host, command], stdout=stdout, stderr=stderr, check=True, timeout=args.duration + 30)
        time.sleep(0.3)
        sub.kill()
        sub.wait(timeout=3)
        lines = (out / "mqtt.log").read_text().splitlines()
        rows = [json.loads(line.split(" ", 1)[1]) for line in lines if line.startswith("test/ruuvi-live/SENSOR ")]
        console = [json.loads(line) for line in (out / "console.jsonl").read_text().splitlines()]
        assert len(rows) >= 3, "Too few live packets; inspect receiver.log and mqtt.log"
        assert rows == console, "MQTT capture differs from console output"
        assert len({r["MeasurementSequence"] for r in rows}) >= 3, "Measurements are not advancing"
        for suffix in ("LWT", "AVAILABILITY"):
            retained = subprocess.check_output(["mosquitto_sub", "-h", "127.0.0.1", "-p", str(port), "-t", f"test/ruuvi-live/{suffix}", "-C", "1", "-W", "3"], text=True, timeout=5)
            assert retained.strip() == "Offline", (suffix, retained)
        summary = {"samples": len(rows), "distinct_sequences": len({r["MeasurementSequence"] for r in rows}), "rssi_min": min(r["RSSI"] for r in rows if r["RSSI"] is not None), "rssi_max": max(r["RSSI"] for r in rows if r["RSSI"] is not None), "first": rows[0], "last": rows[-1], "mqtt_equals_console": True, "retained_shutdown_offline": True}
        (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
        print(json.dumps(summary, indent=2))
        state = run(ssh + [args.host, "bluetoothctl show"], capture_output=True, text=True).stdout
        (out / "adapter-after.txt").write_text(state)
        assert "Discovering: no" in state, state
    finally:
        for child in (sub, broker):
            if child is not None:
                if child.poll() is None:
                    child.kill()
                child.wait(timeout=3)


if __name__ == "__main__":
    main()

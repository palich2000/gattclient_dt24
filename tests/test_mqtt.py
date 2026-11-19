#!/usr/bin/env python3
"""Exercise transport against an isolated loopback Mosquitto broker."""
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def wait_port(port):
    for _ in range(50):
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.1):
                return
        except OSError:
            time.sleep(0.1)
    raise AssertionError("Test broker did not start")


def main():
    port = free_port()
    with tempfile.TemporaryDirectory(prefix="ruuvi-mqtt-test-") as tmp:
        config = Path(tmp) / "mosquitto.conf"
        broker_settings = f"listener {port} 127.0.0.1\nallow_anonymous true\npersistence false\n"
        tls_args = []
        client_args = []
        if "--tls" in sys.argv[2:]:
            cert, key = Path(tmp) / "server.crt", Path(tmp) / "server.key"
            subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-sha256", "-nodes", "-keyout", str(key), "-out", str(cert), "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost,IP:127.0.0.1", "-days", "1"], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            # Mosquitto 1.6 enables TLS via cafile/capath, unlike 2.x.
            broker_settings += f"cafile {cert}\ncertfile {cert}\nkeyfile {key}\n"
            tls_args = ["--cafile", str(cert)]
            client_args = [str(cert)]
        config.write_text(broker_settings)
        broker_log = open(Path(tmp) / "broker.log", "w+")
        broker = subprocess.Popen(["mosquitto", "-c", str(config), "-v"], stdout=broker_log, stderr=broker_log)
        sub = None
        try:
            wait_port(port)
            sub = subprocess.Popen(["mosquitto_sub", "-h", "127.0.0.1", "-p", str(port), "-t", "test/transport/#", "-v", "-C", "3", "-W", "10"] + tls_args, stdout=subprocess.PIPE, text=True)
            time.sleep(0.3)
            subprocess.run([sys.argv[1], str(port)] + client_args, check=True, timeout=10)
            lines = sub.communicate(timeout=10)[0].splitlines()
            assert sub.returncode == 0, lines
            if "dt24" in Path(sys.argv[1]).name:
                assert lines[0] == 'test/transport/LWT Online' and lines[-1] == 'test/transport/LWT Offline', lines
                topic, payload = lines[1].split(" ", 1)
                sample = json.loads(payload)
                assert topic == "test/transport/STATE" and sample["Current"] == 2.5 and sample["Voltage"] == 12 and sample["Power"] == 30, lines
            else:
                assert lines == ['test/transport/LWT Online', 'test/transport/SENSOR {"test":true}', 'test/transport/LWT Offline'], lines
            retained = subprocess.check_output(["mosquitto_sub", "-h", "127.0.0.1", "-p", str(port), "-t", "test/transport/LWT", "-C", "1", "-W", "3"] + tls_args, text=True)
            assert retained.strip() == "Offline", retained
            print("Verified MQTT payload, Online/Offline ordering and retained shutdown state")
        except Exception:
            broker_log.flush()
            broker_log.seek(0)
            print(broker_log.read())
            raise
        finally:
            if sub and sub.poll() is None:
                sub.kill()
                sub.wait(timeout=3)
            broker.kill()
            broker.wait(timeout=3)
            broker_log.close()


if __name__ == "__main__":
    main()

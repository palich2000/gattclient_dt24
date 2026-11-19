#!/usr/bin/env python3
"""Configuration/CLI regression without accessing Bluetooth or MQTT."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary = sys.argv[1]
base = "[bluetooth]\nmac=DE:4D:F8:3F:DA:91\n[mqtt]\nenabled=false\nclient_id=test-config\ntopic_prefix=test/config\n"

def run(config, *args, ok=True):
    result = subprocess.run([binary, "-C", str(config), "--check-config", *args], capture_output=True, text=True, timeout=5)
    assert (result.returncode == 0) == ok, (result.returncode, result.stdout, result.stderr)
    assert "secret-test-value" not in result.stdout + result.stderr
    return result.stdout

with tempfile.TemporaryDirectory(prefix="gatt-config-test-") as tmp:
    config = Path(tmp) / "test.ini"
    config.write_text(base)
    os.chmod(config, 0o600)
    out = run(config)
    assert "Configuration valid" in out, out
    for extra in ["[app]\npublish_interval=0\n", "[unknown]\nfield=1\n", "[app]\nstale_timeout=1\n"]:
        config.write_text(base + extra)
        run(config, ok=False)
    for setting in ["address_type=typo", "security=typo", "mtu=999999"]:
        config.write_text(base.replace("[bluetooth]", "[bluetooth]\n" + setting))
        run(config, ok=False)
    config.write_text(base.replace("enabled=false", "enabled=maybe"))
    run(config, ok=False)
    config.write_text(base.replace("enabled=false", "enabled=false\nusername=test\npassword=secret-test-value"))
    os.chmod(config, 0o644)
    run(config, ok=False)
    os.chmod(config, 0o600)
    run(config)
    config.write_text(base + "secret-test-value\n")
    run(config, ok=False)
    config.write_text(base)
    run(config, "-d", "invalid-mac", ok=False)
    if Path(binary).name == "gattclient":
        for arguments in [("-i", "hci5", "-d", "11:22:33:44:55:66", "-H", "override"), ("-H", "override", "-d", "11:22:33:44:55:66", "-i", "hci5")]:
            out = run(config, *arguments)
            assert "hci5" in out and "11:22:33:44:55:66" in out and "tele/override" in out, out
        result = subprocess.run([binary, "-d", "11:22:33:44:55:66", "--check-config"], capture_output=True, text=True, timeout=5)
        assert result.returncode != 0, "MQTT must not silently use embedded credentials"
print("Configuration errors, private credentials, CLI precedence: OK")

#!/usr/bin/env python3
"""Configuration-only checks: never touches a Bluetooth adapter."""
import subprocess
import sys
import tempfile
from pathlib import Path

base = '''[bluetooth]
mac=70:BF:48:9E:1B:42
mtu=48
[mqtt]
host=localhost
client_id=combined-test
topic_prefix=tele/hass-batt
[ruuvi]
mac=DE:4D:F8:3F:DA:91
topic_prefix=tele/ruuvi1
[app]
console_json=false
'''
with tempfile.TemporaryDirectory(prefix='combined-config-') as tmp:
    p = Path(tmp) / 'config.ini'
    for text, valid in [(base, True), (base.replace('tele/ruuvi1', 'tele/+'), False),
                        (base.replace('DE:4D:F8:3F:DA:91', 'bad'), False),
                        (base.replace('tele/ruuvi1', 'tele/hass-batt'), False)]:
        p.write_text(text)
        r = subprocess.run([sys.argv[1], '-C', str(p), '--check-config'], capture_output=True, text=True)
        assert (r.returncode == 0) == valid, (r.returncode, r.stdout, r.stderr)
    p.write_text(base)
    plain = Path(sys.argv[1]).resolve().with_name('gattclient')
    assert subprocess.run([plain, '-C', str(p), '--check-config']).returncode != 0, 'DT24-only accepted Ruuvi'
    p.write_text(base.replace('mac=DE:4D:F8:3F:DA:91\n', ''))
    subprocess.run([sys.argv[1], '-C', str(p), '--ruuvi', 'DE:4D:F8:3F:DA:91', '--check-config'], check=True)
print('Combined configuration: MAC, independent prefixes and console logging passed')

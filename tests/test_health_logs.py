#!/usr/bin/env python3
"""Exercise the real scanner timer using a wrapped monotonic clock."""
import subprocess
import sys
r=subprocess.run([sys.argv[1]],capture_output=True,text=True,check=True)
text=r.stdout+r.stderr
assert 'Ruuvi health:' in text,text
assert 'new_measurements=1' in text,text
assert 'measurement_age=' in text and 'RSSI=-70' in text,text
print('Minute health timer: new measurements, age and RSSI passed')
if len(sys.argv)>2:
    r=subprocess.run([sys.argv[2]],capture_output=True,text=True,check=True)
    text=r.stdout+r.stderr
    assert 'DT24 health: frames=60' in text and 'fresh=yes' in text,text
    assert 'DT24 health: frames=0' in text and 'fresh=no' in text,text
    assert 'MQTT dt24=disabled ruuvi=disabled' in text,text
    assert '26.20V 0.50A' not in text,text
    assert '[debug]' in text and '26.20V 0.60A' in text,text
    print('Actual DT24 callback: health preserved; readings hidden at INFO and visible at DEBUG')

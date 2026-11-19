#!/usr/bin/env python3
"""Real isolated broker: dual prefixes, independent LWT, normal/crash exits."""
import json
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from test_mqtt import free_port, wait_port

port=free_port()
with tempfile.TemporaryDirectory(prefix='dual-mqtt-') as tmp:
    conf=Path(tmp)/'broker.conf'
    conf.write_text(f'listener {port} 127.0.0.1\nallow_anonymous true\npersistence false\n')
    broker=subprocess.Popen(['mosquitto','-c',str(conf)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    child=sub=None
    try:
        wait_port(port)
        args=['mosquitto_sub','-h','127.0.0.1','-p',str(port)]
        sub=subprocess.Popen(args+['-t','test/#','-v','-W','6'],stdout=subprocess.PIPE,text=True)
        time.sleep(.3)
        subprocess.run([sys.argv[1],str(port)],check=True,timeout=8)
        lines=sub.communicate(timeout=8)[0].splitlines()
        assert 'test/ruuvi1/SENSOR {"MeasurementSequence":123}' in lines,lines
        for prefix in ('test/dt24','test/ruuvi1'):
            assert f'{prefix}/LWT Online' in lines and f'{prefix}/LWT Offline' in lines,lines
        assert 'test/ruuvi1/AVAILABILITY Online' in lines and 'test/ruuvi1/AVAILABILITY Offline' in lines,lines
        state=[json.loads(s.split(' ',1)[1]) for s in lines if s.startswith('test/dt24/STATE ')]
        assert state and state[0]['Voltage']==12 and state[0]['Power']==30,lines
        child=subprocess.Popen([sys.argv[1],str(port),'crash'],stdout=subprocess.PIPE,text=True)
        assert child.stdout is not None
        assert child.stdout.readline().strip()=='ready'
        child.kill();child.wait(timeout=3);time.sleep(.5)
        for prefix in ('test/dt24','test/ruuvi1'):
            value=subprocess.check_output(args+['-t',prefix+'/LWT','-C','1','-W','3'],text=True)
            assert value.strip()=='Offline',value
        print('Dual MQTT prefixes, payloads, clean shutdown and both crash LWTs passed')
    finally:
        for p in (child,sub,broker):
            if p and p.poll() is None:p.kill();p.wait(timeout=3)

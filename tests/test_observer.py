"""Exercise observer routing with captured-format MQTT client output."""
import io
import json
from contextlib import redirect_stdout
from pathlib import Path
import runpy
import sys
import tempfile
from types import SimpleNamespace
from unittest.mock import patch

script = Path(__file__).resolve().parents[1]/'scripts/check-combined-mqtt.py'
with tempfile.TemporaryDirectory() as tmp:
    config = Path(tmp)/'probe.ini'
    for section, sensor in [('', 'test/dt/RUUVI'), ('[ruuvi]\ntopic_prefix=\n', 'test/dt/RUUVI'),
                            ('[ruuvi]\ntopic_prefix=test/tag\n', 'test/tag')]:
        config.write_text('[mqtt]\nhost=localhost\nport=1883\ntopic_prefix=test/dt\n'+section)
        output = ('received CONNACK (0)\nreceived SUBACK\ntest/dt/STATE {"Time":"a"}\n'
                  +sensor+'/SENSOR {"MeasurementSequence":17}\n'+sensor+'/AVAILABILITY Online\n')
        with patch.object(sys,'argv',[str(script),'-C',str(config),'--require-data']), \
             patch('subprocess.run',return_value=SimpleNamespace(stdout=output)) as sub, redirect_stdout(io.StringIO()) as buf:
            runpy.run_path(str(script),run_name='__main__')
            args=sub.call_args.args[0]
            assert 'test/dt/#' in args and '/#' not in args
            assert sensor.startswith('test/dt/') or sensor+'/#' in args
            report=json.loads(buf.getvalue())
            assert report['devices'][sensor]['last']['MeasurementSequence']==17
            assert report['availability'][sensor+'/AVAILABILITY']=='Online'

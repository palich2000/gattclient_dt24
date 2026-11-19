#!/usr/bin/env python3
"""Read-only MQTT verification; credentials never appear in process arguments."""
import argparse
import configparser
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    base=Path(__file__).resolve().parent
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('-C','--config',default=str(base/'combined.ini'))
    parser.add_argument('--seconds',type=int,default=35)
    parser.add_argument('--require-data',action='store_true')
    args=parser.parse_args()
    if not 1<=args.seconds<=600:parser.error('seconds must be 1..600')
    cfg=configparser.ConfigParser(interpolation=None)
    if not cfg.read(args.config):raise RuntimeError('Cannot read configuration')
    def value(section,key):
        return re.sub(r'\\([\\snrt])',lambda m:{'\\':'\\','s':' ','n':'\n','r':'\r','t':'\t'}[m.group(1)],cfg[section].get(key,''))
    fields=[('-h',value('mqtt','host')),('-p',value('mqtt','port'))]
    for flag,key in [('-u','username'),('-P','password')]:
        if value('mqtt',key):fields.append((flag,value('mqtt',key)))
    if cfg.getboolean('mqtt','tls',fallback=False):
        for flag,key in [('--cafile','ca_file'),('--cert','cert_file'),('--key','key_file')]:
            if value('mqtt',key):fields.append((flag,value('mqtt',key)))
    if any('\n' in v or '\r' in v for _,v in fields):raise RuntimeError('Multiline client option not supported')
    dt=value('mqtt','topic_prefix') or 'tele/main-batt'
    separate=value('ruuvi','topic_prefix') if cfg.has_section('ruuvi') else ''
    sensor=separate or dt+'/RUUVI'
    telemetry={dt+'/STATE':dt,sensor+'/SENSOR':sensor}
    status={dt+'/LWT',sensor+'/AVAILABILITY'}
    if separate:status.add(sensor+'/LWT')
    with tempfile.TemporaryDirectory(prefix='mqtt-observe-',dir=str(base)) as tmp:
        p=Path(tmp)/'mosquitto_sub'
        fd=os.open(str(p),os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
        with os.fdopen(fd,'w') as f:
            for flag,v in fields:f.write(flag+' '+v+'\n')
        command=['mosquitto_sub','-d','-v','-W',str(args.seconds)]
        for prefix in ([dt,separate] if separate else [dt]):command+=['-t',prefix+'/#']
        result=subprocess.run(command,env=dict(os.environ,XDG_CONFIG_HOME=tmp),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,universal_newlines=True,timeout=args.seconds+5)
    output=result.stdout
    authenticated='received CONNACK (0)' in output and 'received SUBACK' in output
    samples={prefix:[] for prefix in (dt,sensor)};availability={}
    for line in output.splitlines():
        if ' ' not in line:continue
        topic,payload=line.split(' ',1)
        if topic in telemetry:
            try:samples[telemetry[topic]].append(json.loads(payload))
            except ValueError:pass
        if topic in status:availability[topic]=payload
    report={'authenticated':authenticated,'availability':availability,'devices':{}}
    for prefix,rows in samples.items():
        report['devices'][prefix]={'messages':len(rows),'unique_times':len({r.get('Time') for r in rows}),'unique_sequences':len({r['MeasurementSequence'] for r in rows if 'MeasurementSequence' in r}),'last':rows[-1] if rows else None}
    print(json.dumps(report,indent=2))
    if not authenticated or (args.require_data and not all(samples.values())):raise SystemExit(1)

if __name__=='__main__':main()

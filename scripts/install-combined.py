#!/usr/bin/env python3
"""Install the staged, hash-verified combined BLE receiver on zigbeegate.
Run --check unprivileged first. No modification of rc.local or old binaries.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import pwd
import shutil
import socket
import subprocess
import tempfile
import time

BASE=Path(__file__).resolve().parent
UNIT='ble-mqtt.service'
TARGETS={'gattclient-ruuvi':Path('/usr/local/libexec/ble-mqtt/gattclient-ruuvi'),
         'combined.ini':Path('/etc/ble-mqtt/combined.ini'),
         UNIT:Path('/etc/systemd/system')/UNIT}

def command(*args):
    return subprocess.run(args,check=True)

def check_release():
    if BASE != Path('/root/ble-mqtt-release'):
        raise RuntimeError('Use the reviewed root-owned release under /root/ble-mqtt-release')
    for path in [BASE.parent, BASE, Path(__file__), BASE/'manifest.json', *(BASE/n for n in TARGETS)]:
        st=path.lstat()
        if path.is_symlink() or st.st_uid != 0 or st.st_mode & 0o022:
            raise RuntimeError('Untrusted release path: '+str(path))

def preflight():
    if socket.gethostname()!='zigbeegate':raise RuntimeError('This deployment is for zigbeegate only')
    account=pwd.getpwnam('palich')
    manifest=json.loads((BASE/'manifest.json').read_text())
    if set(manifest)!=set(TARGETS):raise RuntimeError('Unexpected manifest entries')
    for name,target in TARGETS.items():
        source=BASE/name
        if source.is_symlink() or not source.is_file():raise RuntimeError('Invalid staged file: '+name)
        if hashlib.sha256(source.read_bytes()).hexdigest()!=manifest[name]:raise RuntimeError('Staged checksum mismatch: '+name)
        if target.exists() or target.is_symlink():raise RuntimeError('Refusing to overwrite '+str(target))
    if (BASE/'combined.ini').stat().st_mode & 0o077:raise RuntimeError('Private configuration must be mode 0600')
    rc=Path('/etc/rc.local')
    if rc.exists() and any(not s.lstrip().startswith('#') and ('hass_bat' in s or 'gattclient' in s) for s in rc.read_text().splitlines()):
        raise RuntimeError('Old rc.local startup is still enabled')
    for p in Path('/proc').glob('[0-9]*/comm'):
        try:name=p.read_text().strip()
        except OSError:continue
        if name in ('gattclient','gattclient-ruuvi','ruuvi-mqtt'):raise RuntimeError('An existing BLE reader is running: '+name)
    if os.geteuid()!=0:
        command(str(BASE/'gattclient-ruuvi'),'-C',str(BASE/'combined.ini'),'--check-config')
    # Validate against the staged paths before installing anything.
    with tempfile.TemporaryDirectory(prefix='unit-check-',dir=str(BASE)) as d:
        unit=(BASE/UNIT).read_text().replace(str(TARGETS['gattclient-ruuvi']),str(BASE/'gattclient-ruuvi')).replace(str(TARGETS['combined.ini']),str(BASE/'combined.ini'))
        path=Path(d)/UNIT;path.write_text(unit)
        command('systemd-analyze','verify',str(path))
    print('Preflight passed: source hashes, config, unit and no duplicate reader',flush=True)
    return account

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args()
    if os.geteuid()==0:
        check_release()
        if args.check:raise RuntimeError('Run --check without sudo')
    account=preflight()
    if args.check:return
    if os.geteuid()!=0:raise RuntimeError('Installation requires sudo')
    installed=[]
    try:
        for name,target in TARGETS.items():
            target.parent.mkdir(parents=True,exist_ok=True)
            if any(p.is_symlink() for p in [target.parent,*target.parent.parents]):raise RuntimeError('Symlink in destination path')
            mode=0o600 if name=='combined.ini' else 0o755 if name=='gattclient-ruuvi' else 0o644
            fd=os.open(str(target),os.O_WRONLY|os.O_CREAT|os.O_EXCL,mode)
            installed.append(target)
            with os.fdopen(fd,'wb') as out:out.write((BASE/name).read_bytes())
            os.chmod(str(target),mode)
            os.chown(str(target),account.pw_uid if name=='combined.ini' else 0,account.pw_gid if name=='combined.ini' else 0)
            if hashlib.sha256(target.read_bytes()).hexdigest()!=json.loads((BASE/'manifest.json').read_text())[name]:raise RuntimeError('Installed checksum mismatch')
        command('runuser','-u','palich','--',str(TARGETS['gattclient-ruuvi']),'-C',str(TARGETS['combined.ini']),'--check-config')
        command('systemctl','daemon-reload')
        command('systemctl','enable','--now',UNIT)
        time.sleep(5)
        command('systemctl','is-active',UNIT)
        command('systemctl','show',UNIT,'-p','User','-p','MainPID','-p','NRestarts','-p','AmbientCapabilities')
        command('journalctl','-u',UNIT,'-n','25','--no-pager')
        print('Installed and started. Verify fresh MQTT messages from BOTH devices; active alone is not acceptance.',flush=True)
    except BaseException:
        subprocess.run(['systemctl','disable','--now',UNIT],check=False)
        for path in reversed(installed):path.unlink()
        subprocess.run(['systemctl','daemon-reload'],check=False)
        raise

if __name__=='__main__':
    try:main()
    except Exception as error:
        print('ERROR: '+str(error),flush=True)
        raise SystemExit(1)

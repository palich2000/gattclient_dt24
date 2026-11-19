#!/usr/bin/env python3
"""Install the tested quiet-log build with rollback; preserve configuration."""
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import time
base=Path(__file__).resolve().parent
source=base/'gattclient-ruuvi-quiet'
target=Path('/usr/local/libexec/ble-mqtt/gattclient-ruuvi')
backup=target.with_name('gattclient-ruuvi.before-quiet')
staged=target.with_name('gattclient-ruuvi.quiet-new')
old_hash='09fc4f1ab0ac0e444ea65bd79027b33c970bf53a80c0af631ed8d053aaa62a6a'
new_hash='13ae44b7849a3d8c0a5e488f65585816417d87f39d2cd8ad952c5ef557c5aaee'
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(*args):return subprocess.run(args,check=True)
if os.geteuid()!=0:raise SystemExit('Run with sudo')
if base != Path('/root/ble-mqtt-release'):
    raise SystemExit('Use the reviewed root-owned release under /root/ble-mqtt-release')
for path in (base.parent,base,Path(__file__),source):
    st=path.lstat()
    if path.is_symlink() or st.st_uid!=0 or st.st_mode & 0o022:
        raise SystemExit('Untrusted release path: '+str(path))
if source.is_symlink() or target.is_symlink() or digest(source)!=new_hash:raise SystemExit('Unexpected artifact')
if digest(target)==new_hash:raise SystemExit('Quiet build already installed')
if digest(target)!=old_hash or backup.exists() or backup.is_symlink() or staged.exists() or staged.is_symlink():raise SystemExit('Unexpected baseline or existing backup/staging file')
with staged.open('xb') as f:f.write(source.read_bytes())
try:
    if digest(staged)!=new_hash:raise SystemExit('Staged checksum mismatch')
    os.chmod(str(staged),0o755)
    run('runuser','-u','palich','--',str(staged),'-C','/etc/ble-mqtt/combined.ini','--check-config')
    with backup.open('xb') as f:f.write(target.read_bytes())
    os.chmod(str(backup),0o755)
except BaseException:
    staged.unlink()
    raise
try:
    os.replace(str(staged),str(target))
    run('systemctl','restart','ble-mqtt.service')
    time.sleep(3)
    run('systemctl','is-active','ble-mqtt.service')
    pid=subprocess.check_output(['systemctl','show','ble-mqtt.service','-p','MainPID','--value'],universal_newlines=True).strip()
    if digest(Path('/proc')/pid/'exe')!=new_hash:raise RuntimeError('Running executable mismatch')
except BaseException:
    shutil.copy2(str(backup),str(staged))
    os.replace(str(staged),str(target))
    subprocess.run(['systemctl','restart','ble-mqtt.service'],check=False)
    raise
print('Quiet build installed and running; config unchanged. Verify sensor reception and minute health logs separately.')

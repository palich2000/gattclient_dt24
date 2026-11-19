#!/usr/bin/env python3
"""Installer transaction tests with isolated paths and no real systemctl."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
from unittest.mock import patch

spec=importlib.util.spec_from_file_location('installer',Path(__file__).resolve().parents[1]/'scripts/install-combined.py')
assert spec and spec.loader
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
for fail in (False,True):
    with tempfile.TemporaryDirectory(prefix='combined-install-test-') as tmp:
        base=Path(tmp)/'stage';base.mkdir()
        targets={n:Path(tmp)/'installed'/n for n in ('gattclient-ruuvi','combined.ini','ble-mqtt.service')}
        for name in targets:(base/name).write_text('fixture '+name)
        (base/'manifest.json').write_text(json.dumps({n:hashlib.sha256((base/n).read_bytes()).hexdigest() for n in targets}))
        events=[]
        def command(*args):
            events.append(args)
            if fail and args[:2]==('systemctl','is-active'):raise RuntimeError('injected service failure')
        account=SimpleNamespace(pw_uid=os.getuid(),pw_gid=os.getgid())
        with patch.object(module,'BASE',base),patch.object(module,'TARGETS',targets),patch.object(module,'check_release'),patch.object(module,'preflight',return_value=account),patch.object(module,'command',side_effect=command),patch.object(module.os,'geteuid',return_value=0),patch.object(module.os,'chown'),patch.object(module.time,'sleep'),patch.object(module.subprocess,'run') as rollback,patch.object(sys,'argv',['install-combined.py']):
            if fail:
                try:module.main()
                except RuntimeError:pass
                else:raise AssertionError('Injected failure not propagated')
                assert not any(p.exists() for p in targets.values())
                assert rollback.call_args_list[0].args[0]==['systemctl','disable','--now','ble-mqtt.service']
            else:
                module.main()
                assert ('runuser','-u','palich','--',str(targets['gattclient-ruuvi']),'-C',str(targets['combined.ini']),'--check-config') in events
                assert all(p.read_bytes()==(base/n).read_bytes() for n,p in targets.items())
                assert targets['combined.ini'].stat().st_mode&0o777==0o600
                assert targets['gattclient-ruuvi'].stat().st_mode&0o777==0o755
                assert ('systemctl','enable','--now','ble-mqtt.service') in events
print('Installer sandbox: copy, permissions, start and rollback passed')

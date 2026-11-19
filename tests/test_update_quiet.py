#!/usr/bin/env python3
"""Exercise the updater with sandbox paths and mocked privileged commands."""
import ast
import hashlib
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[1] / 'scripts' / 'update-quiet.py'

class UpdateTests(unittest.TestCase):
    def exercise(self, scenario):
        with tempfile.TemporaryDirectory(prefix='quiet-update-test-') as tmp:
            root = Path(tmp)
            stage = root / 'stage'
            stage.mkdir(mode=0o700)
            target = root / 'gattclient-ruuvi'
            old, new = b'old test binary', b'new test binary'
            target.write_bytes(new if scenario == 'already' else old)
            (stage / 'gattclient-ruuvi-quiet').write_bytes(b'bad' if scenario == 'bad_hash' else new)
            (stage/'gattclient-ruuvi-quiet').chmod(0o666 if scenario=='writable' else 0o644)
            script_copy=stage/'update-quiet.py'
            script_copy.write_bytes(SCRIPT.read_bytes())
            script_copy.chmod(0o644)
            proc = root / 'proc' / '123'
            proc.mkdir(parents=True)
            (proc / 'exe').write_bytes(old if scenario == 'wrong_process' else new)
            replacements = {
                'base': 'Path(%r)' % str(stage),
                'target': 'Path(%r)' % str(target),
                'old_hash': repr(hashlib.sha256(old).hexdigest()),
                'new_hash': repr(hashlib.sha256(new).hexdigest()),
            }
            class Sandbox(ast.NodeTransformer):
                def visit_Assign(self, node):
                    if len(node.targets) == 1 and isinstance(node.targets[0], ast.Name):
                        name = node.targets[0].id
                        if name in replacements:
                            return ast.copy_location(ast.parse(name+'='+replacements[name]).body[0], node)
                    return self.generic_visit(node)
                def visit_Constant(self, node):
                    if node.value == '/root/ble-mqtt-release':
                        return ast.copy_location(ast.Constant(str(stage)), node)
                    if node.value == '/proc':
                        return ast.copy_location(ast.Constant(str(root/'proc')), node)
                    return node
            tree = ast.fix_missing_locations(Sandbox().visit(ast.parse(SCRIPT.read_text())))
            calls = []
            def run(args, **kwargs):
                args = list(args)
                calls.append(args)
                if scenario == 'startup_failure' and args[:2] == ['systemctl', 'is-active']:
                    raise subprocess.CalledProcessError(3, args)
                return subprocess.CompletedProcess(args, 0)
            error = None
            # Model root ownership of this sandbox release, not system paths.
            real_lstat=Path.lstat
            def root_stat(p):
                from types import SimpleNamespace
                st=real_lstat(p)
                return SimpleNamespace(st_uid=0,st_mode=st.st_mode)
            with patch.object(Path,'lstat',root_stat), patch('os.geteuid', return_value=0), patch('subprocess.run', side_effect=run), patch('subprocess.check_output', return_value='123\n'), patch('time.sleep'):
                try:
                    exec(compile(tree, str(SCRIPT), 'exec'), {'__file__': str(script_copy), '__name__': '__main__'})
                except (SystemExit, RuntimeError, subprocess.CalledProcessError) as exc:
                    error = exc
            backup = target.with_name('gattclient-ruuvi.before-quiet')
            if scenario == 'success':
                self.assertIsNone(error)
                self.assertEqual(calls[0][:4], ['runuser','-u','palich','--'])
                self.assertEqual(calls[0][4],str(target.with_name('gattclient-ruuvi.quiet-new')))
                self.assertEqual(target.read_bytes(), new)
                self.assertEqual(backup.read_bytes(), old)
            elif scenario in ('startup_failure', 'wrong_process'):
                self.assertIsNotNone(error)
                self.assertEqual(target.read_bytes(), old)
                self.assertEqual(backup.read_bytes(), old)
                self.assertEqual(sum(c[:2] == ['systemctl', 'restart'] for c in calls), 2)
            else:
                self.assertIsInstance(error, SystemExit)
                self.assertEqual(calls, [])
                self.assertFalse(backup.exists())
                self.assertEqual(target.read_bytes(), new if scenario == 'already' else old)
            self.assertFalse(target.with_name('gattclient-ruuvi.quiet-new').exists())

    def test_success(self): self.exercise('success')
    def test_startup_failure_rolls_back(self): self.exercise('startup_failure')
    def test_wrong_running_binary_rolls_back(self): self.exercise('wrong_process')
    def test_bad_artifact_refused(self): self.exercise('bad_hash')
    def test_writable_artifact_refused(self): self.exercise('writable')
    def test_already_installed_refused(self): self.exercise('already')

if __name__ == '__main__':
    unittest.main()

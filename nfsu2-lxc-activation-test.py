#!/usr/bin/env python3
"""Exercise release activation and rollback using only disposable PC files."""

import importlib.util
import json
from pathlib import Path
import socket
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('nfsu2_activation', ROOT / 'nfsu2-lxc-activate-remote.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ActivationTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='nfsu2-activation-test-')
        self.root = Path(self.temp.name)
        self.releases = self.root / 'releases'
        self.releases.mkdir()
        self.old, self.new = self.releases / 'old', self.releases / 'new'
        self.old.mkdir()
        self.new.mkdir()
        (self.old / 'nfsu2.json').write_text(json.dumps({'rev': 'oldrev'}))
        (self.root / 'current').symlink_to('releases/old')
        self.state = self.root / 'state'
        self.state.mkdir()
        (self.state / 'save').write_text('original')
        self.sock = socket.socket(socket.AF_UNIX)
        self.sock.bind(str(self.state / 'control.sock'))
        self.events = []
        self.fail_new = False
        self.fail_initial_backup = False
        self.fail_final_backup = False
        self.sync_count = 0
        self.patches = [patch.object(module, 'ROOT', self.root), patch.object(module, 'RELEASES', self.releases),
                        patch.object(module, 'STATE', self.state), patch.object(module, 'run', self.fake_run),
                        patch.object(module, 'request', lambda *a, **k: b'{"players":0}'),
                        patch.object(module, 'wait_http', self.fake_health)]
        for p in self.patches:
            p.start()

    def tearDown(self):
        for p in reversed(self.patches):
            p.stop()
        self.sock.close()
        self.temp.cleanup()

    def fake_run(self, args, **kwargs):
        if args[0] == 'systemctl':
            self.events.append(tuple(args[1:]))
            if args[1] == 'start' and (self.root / 'current').resolve() == self.new and self.fail_new:
                (self.state / 'save').write_text('modified-by-failed-release')
            return subprocess.CompletedProcess(args, 0)
        if args[0] == 'rsync':
            self.sync_count += 1
            if self.fail_initial_backup and self.sync_count == 1:
                raise subprocess.CalledProcessError(1, args)
            if self.fail_final_backup and self.sync_count == 2:
                raise subprocess.CalledProcessError(1, args)
        return subprocess.run(args, check=True, **kwargs)

    def fake_health(self, port, rev, *args, **kwargs):
        if rev == 'newrev' and self.fail_new:
            raise RuntimeError('simulated new-release health failure')
        return {'rev': rev, 'module': 'test'}

    def test_success_keeps_backup_and_switches(self):
        module.activate(self.new, 'newrev', 'new')
        self.assertEqual((self.root / 'current').resolve(), self.new)
        backups = list((self.root / 'backups').glob('pre-*'))
        self.assertEqual(len(backups), 1)
        self.assertEqual((backups[0] / 'save').read_text(), 'original')
        self.assertFalse((backups[0] / 'control.sock').exists())

    def test_failed_new_release_restores_state_and_previous_release(self):
        self.fail_new = True
        with self.assertRaisesRegex(RuntimeError, 'previous release was restored'):
            module.activate(self.new, 'newrev', 'new')
        self.assertEqual((self.root / 'current').resolve(), self.old)
        self.assertEqual((self.state / 'save').read_text(), 'original')
        failed = list((self.root / 'backups').glob('failed-release-*'))
        self.assertEqual((failed[0] / 'save').read_text(), 'modified-by-failed-release')

    def test_failed_final_backup_restarts_old_without_overwriting_state(self):
        self.fail_final_backup = True
        with self.assertRaisesRegex(RuntimeError, 'previous release was restored'):
            module.activate(self.new, 'newrev', 'new')
        self.assertEqual((self.root / 'current').resolve(), self.old)
        self.assertEqual((self.state / 'save').read_text(), 'original')
        self.assertIn(('start', 'nfsu2.service'), self.events)
        self.assertFalse(list((self.root / 'backups').glob('failed-release-*')))

    def test_failed_initial_backup_never_stops_the_server(self):
        self.fail_initial_backup = True
        with self.assertRaises(subprocess.CalledProcessError):
            module.activate(self.new, 'newrev', 'new')
        self.assertEqual((self.root / 'current').resolve(), self.old)
        self.assertEqual((self.state / 'save').read_text(), 'original')
        self.assertNotIn(('stop', 'nfsu2.service'), self.events)

    def test_already_active_release_does_not_restart(self):
        (self.new / 'nfsu2.json').write_text(json.dumps({'rev': 'newrev'}))
        (self.root / 'current').unlink()
        (self.root / 'current').symlink_to('releases/new')
        with patch.object(module, 'verify_http', lambda *a, **k: {'rev': 'newrev', 'module': 'test'}):
            module.activate(self.new, 'newrev', 'new')
        self.assertNotIn(('stop', 'nfsu2.service'), self.events)
        self.assertFalse((self.root / 'backups').exists())

    def test_prune_keeps_the_newest_backups_and_releases_and_the_live_ones(self):
        import os
        backups = self.root / 'backups'
        backups.mkdir()
        made = []
        for i in range(6):
            for folder, name in ((backups, f'pre-{i:08d}-x'), (backups, f'failed-release-{i}'), (self.releases, f'rel{i}')):
                (folder / name).mkdir()
                os.utime(folder / name, (1000 + i, 1000 + i))
                made.append(folder / name)
        for i, path in enumerate((self.old, self.new)):
            os.utime(path, (10 + i, 10 + i))           # the live and previous releases are the oldest of all
        (self.releases / 'link').symlink_to('rel0')
        module.prune(keep_releases={self.old, self.new})
        self.assertEqual(sorted(p.name for p in backups.glob('pre-*')), ['pre-00000003-x', 'pre-00000004-x', 'pre-00000005-x'])
        self.assertEqual(sorted(p.name for p in backups.glob('failed-release-*')), ['failed-release-4', 'failed-release-5'])
        self.assertEqual(sorted(p.name for p in self.releases.iterdir()),
                         ['link', 'new', 'old', 'rel1', 'rel2', 'rel3', 'rel4', 'rel5'])

    def test_success_prunes_and_keeps_the_previous_release(self):
        import os
        for i in range(8):
            (self.releases / f'rel{i}').mkdir()
            os.utime(self.releases / f'rel{i}', (2000 + i, 2000 + i))
        os.utime(self.old, (5, 5))
        module.activate(self.new, 'newrev', 'new')
        names = {p.name for p in self.releases.iterdir()}
        self.assertIn('old', names)
        self.assertIn('new', names)
        self.assertEqual(len(names), module.KEEP_RELEASES + 1)   # the new one is among the newest; the old one is kept beyond them


if __name__ == '__main__':
    unittest.main()

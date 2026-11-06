#!/usr/bin/env python3
"""Run on the LXC: preflight or activate a tested, already-staged release."""

import argparse
import base64
import fcntl
import hashlib
import json
import os
from pathlib import Path
import pwd
import re
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
import urllib.request

ROOT = Path('/srv/nfsu2')
RELEASES = ROOT / 'releases'
STATE = ROOT / 'state'
GAME = ROOT / 'game'
HOST = 'nfsu2.ironwingnetworks.com.au'
SERVICE = 'nfsu2.service'
TUNNEL = 'nfsu2-tunnel.service'
OPENER = urllib.request.build_opener(urllib.request.ProxyHandler({}))


def run(args, **kwargs):
    return subprocess.run(args, check=True, **kwargs)


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def request(port, path, headers=None):
    req = urllib.request.Request(f'http://127.0.0.1:{port}{path}', headers={'Host': HOST, **(headers or {})})
    with OPENER.open(req, timeout=4) as response:
        return response.read()


def websocket(port):
    key = base64.b64encode(os.urandom(16)).decode('ascii')
    request_bytes = (f'GET /lan HTTP/1.1\r\nHost: {HOST}\r\nOrigin: https://{HOST}\r\n'
                     f'Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: {key}\r\n'
                     'Sec-WebSocket-Version: 13\r\n\r\n').encode('ascii')
    with socket.create_connection(('127.0.0.1', port), timeout=4) as s:
        s.sendall(request_bytes)
        response = b''
        while b'\r\n\r\n' not in response and len(response) < 16384:
            part = s.recv(4096)
            if not part:
                break
            response += part
    if not response.startswith(b'HTTP/1.1 101 '):
        raise RuntimeError('WebSocket upgrade failed')


def verify_http(port, expected_rev, *, game_data=False):
    manifest = json.loads(request(port, '/game.json'))
    if manifest.get('rev') != expected_rev or not manifest.get('module'):
        raise RuntimeError(f'unexpected game module revision: {manifest.get("rev")}')
    if b'IronWing' not in request(port, '/'):
        raise RuntimeError('the IronWing homepage was not served')
    websocket(port)
    if game_data:
        actual = request(port, '/data/SPEED2.EXE', {'Range': 'bytes=0-1023', 'Accept-Encoding': 'identity'})
        with (GAME / 'SPEED2.EXE').open('rb') as f:
            if len(actual) != 1024 or actual != f.read(1024):
                raise RuntimeError('the game-data byte range did not match')
    return manifest


def wait_http(port, rev, proc=None, *, game_data=False):
    last = None
    for _ in range(60):
        if proc is not None and proc.poll() is not None:
            raise RuntimeError(f'test server exited with status {proc.returncode}')
        try:
            return verify_http(port, rev, game_data=game_data)
        except Exception as e:
            last = e
            time.sleep(.5)
    raise RuntimeError(f'server failed health checks: {last}')


def sync(source, dest):
    run(['rsync', '-a', '--delete', '--exclude=/control.sock', str(source) + '/', str(dest) + '/'])


def replace_current(target):
    temp = ROOT / f'.current-deploy-{os.getpid()}'
    os.symlink(target, temp)
    try:
        os.replace(temp, ROOT / 'current')
    finally:
        temp.unlink(missing_ok=True)


def staged_release(key):
    if not re.fullmatch(r'[0-9a-f]{40}-[0-9a-f]{12}-[0-9a-f]{12}', key):
        raise RuntimeError('invalid staged release identifier')
    release = RELEASES / key
    if not release.is_dir() or release.resolve().parent != RELEASES.resolve():
        raise RuntimeError('staged release does not exist or is not in the release directory')
    run(['sha256sum', '--quiet', '-c', 'SHA256SUMS'], cwd=release)
    rev = (release / 'BUILD_REVISION').read_text().strip()
    game_sha = (release / 'BUILD_GAME_SHA256').read_text().strip()
    dll_sha = (release / 'BUILD_SERVER_DLL_SHA256').read_text().strip()
    tested = (release / 'BUILD_TESTED').read_text().strip()
    if tested != 'web lan-wasm rust browser-settings quickrace' or not key.startswith(rev + '-'):
        raise RuntimeError('release does not record the required tests or revision')
    if not re.fullmatch(r'[0-9a-f]{64}', game_sha) or not re.fullmatch(r'[0-9a-f]{64}', dll_sha):
        raise RuntimeError('release game-input checksums are invalid')
    if key != f'{rev}-{game_sha[:12]}-{dll_sha[:12]}':
        raise RuntimeError('release identifier does not match its game inputs')
    if sha(GAME / 'SPEED2.EXE') != game_sha or sha(GAME / 'server.dll') != dll_sha:
        raise RuntimeError('LXC game executable or server.dll differs from the PC build; do not activate')
    manifest = json.loads((release / 'nfsu2.json').read_text())
    expected_rev = manifest.get('rev', '')
    if not expected_rev or not rev.startswith(expected_rev):
        raise RuntimeError('module manifest revision does not match the built commit')
    libs = run(['ldd', str(release / 'nfsu2-server')], capture_output=True, text=True).stdout
    if 'not found' in libs:
        raise RuntimeError('server binary has missing libraries')
    return release, expected_rev


def preflight(release, expected_rev):
    account = pwd.getpwnam('nfsu2')
    folder = Path(tempfile.mkdtemp(prefix='.preflight-', dir=ROOT))
    os.chown(folder, account.pw_uid, account.pw_gid)
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        port = s.getsockname()[1]
    env = {k: v for k, v in os.environ.items() if not k.startswith('NFSU2_')}
    env.update({'NFSU2_REPO': str(ROOT), 'NFSU2_COMPRESS_THREADS': '0'})
    args = ['runuser', '-u', 'nfsu2', '--', str(release / 'nfsu2-server'), '--data', str(GAME), '--http-port', str(port),
            '--behind-proxy', '--host', HOST, '--real-ip-header', 'cf-connecting-ip', '--state', str(folder),
            '--dist', str(release / 'dist'), '--wasm', str(release / 'nfsu2.wasm')]
    log = tempfile.TemporaryFile(mode='w+')
    proc = None
    try:
        proc = subprocess.Popen(args, cwd=ROOT, env=env, stdin=subprocess.DEVNULL, stdout=log, stderr=log, start_new_session=True)
        wait_http(port, expected_rev, proc, game_data=True)
        print(f'Isolated LXC preflight passed for {expected_rev}.', flush=True)
    except Exception:
        log.seek(0)
        print('Preflight server output (last lines):', *log.read().splitlines()[-15:], sep='\n', file=sys.stderr)
        raise
    finally:
        if proc and proc.poll() is None:
            try:
                os.killpg(proc.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                proc.wait(timeout=20)
            except subprocess.TimeoutExpired as e:
                raise RuntimeError('preflight server did not stop; test state retained') from e
        log.close()
        if not proc or proc.poll() is not None:
            shutil.rmtree(folder)


def activate(release, expected_rev, key):
    run(['systemctl', 'is-active', '--quiet', SERVICE])
    run(['systemctl', 'is-active', '--quiet', TUNNEL])
    current = ROOT / 'current'
    old_target = os.readlink(current)
    old_release = current.resolve()
    if old_release.parent != RELEASES.resolve():
        raise RuntimeError('current release is outside the release directory')
    old_rev = json.loads((old_release / 'nfsu2.json').read_text())['rev']
    if old_release == release.resolve():
        verify_http(8080, expected_rev)
        print(f'{expected_rev} is already active and healthy; no restart needed.', flush=True)
        return
    players = json.loads(request(8080, '/players')).get('players', 'unknown')
    print(f'Connected players before restart: {players}.', flush=True)
    state_bytes = int(run(['du', '-sk', str(STATE)], capture_output=True, text=True).stdout.split()[0]) * 1024
    if shutil.disk_usage(ROOT).free < 2 * state_bytes + 512 * 1024 * 1024:
        raise RuntimeError('not enough free disk space for state backup and rollback')
    backups = ROOT / 'backups'
    backups.mkdir(mode=0o700, exist_ok=True)
    backup = Path(tempfile.mkdtemp(prefix=f'pre-{key[:8]}-', dir=backups))
    print('Copying state before the restart.', flush=True)
    sync(STATE, backup)
    if os.readlink(current) != old_target:
        raise RuntimeError('current release changed during backup; activation stopped')
    cutover_started = False
    new_started = False
    try:
        cutover_started = True
        run(['systemctl', 'stop', SERVICE], timeout=45)
        sync(STATE, backup)
        if os.readlink(current) != old_target:
            raise RuntimeError('current release changed during cutover')
        replace_current('releases/' + key)
        new_started = True
        run(['systemctl', 'start', SERVICE], timeout=45)
        wait_http(8080, expected_rev, game_data=True)
        run(['systemctl', 'is-active', '--quiet', TUNNEL])
        if not (STATE / 'control.sock').is_socket():
            raise RuntimeError('the server control socket is missing')
    except BaseException as error:
        if not cutover_started:
            raise
        print(f'Activation failed: {error}; restoring the previous release.', file=sys.stderr, flush=True)
        try:
            run(['systemctl', 'stop', SERVICE], timeout=45)
            if new_started:
                failed = Path(tempfile.mkdtemp(prefix='failed-release-', dir=backups))
                sync(STATE, failed)
                sync(backup, STATE)
            replace_current(old_target)
            run(['systemctl', 'start', SERVICE], timeout=45)
            wait_http(8080, old_rev)
            print(f'Previous release {old_rev} restored; backup: {backup}', file=sys.stderr, flush=True)
        except BaseException as rollback_error:
            raise RuntimeError(f'automatic rollback failed; manual recovery required; backup {backup}: {rollback_error}') from error
        raise RuntimeError('activation failed and the previous release was restored') from error
    print(f'Activated {expected_rev}; state backup: {backup}', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('key')
    parser.add_argument('--check-only', action='store_true')
    args = parser.parse_args()
    with open('/run/lock/nfsu2-deploy.lock', 'a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        release, rev = staged_release(args.key)
        preflight(release, rev)
        if args.check_only:
            print('Live service was not restarted.', flush=True)
        else:
            activate(release, rev, args.key)


if __name__ == '__main__':
    try:
        main()
    except Exception as e:
        print(f'nfsu2 activation: {e}', file=sys.stderr)
        sys.exit(1)

# IronWing NFSU2 LXC

The live game server runs on `NFSU2-lxc` at `172.16.10.215`. The SSH alias is case-sensitive on this PC. The public hostname is `nfsu2.ironwingnetworks.com.au`.

The game server listens only on `127.0.0.1:8080` inside the LXC. Its dedicated `nfsu2-tunnel.service` connects to Cloudflare and forwards the public hostname to that listener. There is no `socat` or inbound game port in the live route. Both `nfsu2.service` and `nfsu2-tunnel.service` are enabled at LXC startup.

## Control the live services

From this PC:

```fish
cd ~/Documents/nfsu2-web
./nfsu2-lxc-control status
./nfsu2-lxc-control start
./nfsu2-lxc-control stop
./nfsu2-lxc-control restart
```

`stop` and `restart` disconnect players. `restart` restarts the game server; the tunnel remains running. To inspect logs:

```fish
ssh NFSU2-lxc 'journalctl -u nfsu2.service -n 100 --no-pager'
ssh NFSU2-lxc 'journalctl -u nfsu2-tunnel.service -n 100 --no-pager'
```

The old `nfsu2-background` command now refuses to start the stale PC server by default. Its `status` action checks the LXC. Do not start the old PC server as a rollback until the LXC has stopped and its current state has been preserved and synchronized; the PC's copy is a snapshot from the migration.

## Build, test and deploy from the PC

```fish
cd ~/Documents/nfsu2-web
./nfsu2-lxc-deploy
```

The command clones `main` from `LordMephisto94/nfsu2-web` into an isolated PC directory, copies the PC's game executable and `server.dll` as build inputs, and builds the page, Rust server and game module on the PC. It runs frontend checks, fast web tests, the LAN/WASM test, Rust workspace tests, browser startup/settings checks and a null-renderer quick-race test. It uses half of the PC's CPU threads (at least 4) at low priority by default; set `NFSU2_BUILD_JOBS` to change that. Build tools and caches are kept under `out/lxc-builds` and `out/lxc-build-cache` on the PC.

If a release for the same fork commit and exact game executable and `server.dll` has already passed those checks, the command verifies its checksums and reuses it. A changed commit or game input causes a new build and test run. Other game assets are not automatically copied by an update.

Only if those checks pass does it transfer the release to the LXC. It verifies checksums, compares the game executable and `server.dll` with the LXC copies, and starts a separate temporary LXC server for HTTP, game-data and WebSocket preflight checks. It then backs up the live state, stops the game server, switches the release and restarts it. A failed local activation restores the previous release and state; the failed release's state is retained separately. Finally it checks the public page, module and multiplayer WebSocket. Connected players may be disconnected during the restart.

A public-route check can fail for reasons unrelated to the release. In that case the locally healthy server is left running for investigation rather than automatically restoring an older state over possible new saves. Visual gameplay still needs a real-GPU check.

For separate build and staging steps instead:

```fish
./nfsu2-lxc-build
# Use the release path printed by the build:
./nfsu2-lxc-stage /absolute/path/to/the/built/release
```

Staging alone does **not** restart the live server. To preflight a staged release without restarting, run `./nfsu2-lxc-activate /absolute/path/to/the/built/release --check`. To activate that staged release with the same backup, rollback and public checks, omit `--check`.

## Paths and backups

- Game files: `/srv/nfsu2/game` on the LXC.
- Live state and encrypted cloud saves: `/srv/nfsu2/state`, accessible to the dedicated `nfsu2` account.
- Current release: `/srv/nfsu2/current` (a symlink into `/srv/nfsu2/releases`).
- Before each activation, the live state is backed up under `/srv/nfsu2/backups`; these backups contain private server state. The compressed game-data cache (`state/blocks`, ~650 MB, rebuilt by the server from the game files) is left out of backups and left in place on a rollback.
- Pruning (after a successful activation, `nfsu2-lxc-activate-remote.py`): only the last two deployments are kept: the newest 2 `pre-*` backups, 1 `failed-release-*` copy and 2 releases, plus the live release and the one before it (the disk is kept for players' saves). A pruning error is only a warning. On the PC, `nfsu2-lxc-build` keeps its newest 3 work folders and 5 releases. Change `KEEP_BACKUPS`, `KEEP_FAILED` and `KEEP_RELEASES` there to keep more.
- Tunnel configuration: `/etc/cloudflared/nfsu2.yml`; its tunnel-specific credential is restricted to the tunnel service account.
- Final PC state backup from migration: `out/lxc-migration-20261106/state-backup` on this PC. It is a snapshot, not a live backup.
- The old shared Cloudflare tunnel route is retained for rollback, but the public DNS record points to the new dedicated tunnel.

The LXC and tunnel must be running for the public site to work. Visual gameplay with a real GPU remains a separate check from the automated null-renderer game test.

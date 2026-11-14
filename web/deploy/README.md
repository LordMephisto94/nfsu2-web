# NFS Underground 2 in the browser: the server bundle

This folder runs the game's web server on a Linux VPS, with HTTPS in front, so
friends can play with you in Chrome, without Docker.

It holds the server, the page and the game module. **It does not hold the game.**
You copy your own NFS Underground 2 folder to the VPS yourself.

1. **Nothing to install first.** The server is one static binary, `nfsu2-server`, for
   the VPS's processor: the bundle's name ends in amd64 (Intel, AMD) or arm64.
2. **The bundle.**
   `sudo mkdir -p /opt && sudo tar -C /opt -xzf nfsu2-web-<rev>-<arch>.tar.gz`
   This gives `/opt/nfsu2-web`.
3. **The game folder.** This is the one with `SPEED2.EXE`. From your own computer, inside that folder, copy
   only the game's own files (the server serves nothing else), then make them readable on the VPS:
   `rsync -rlt SPEED2.EXE server.dll server.cfg CARS CREDITS FRONTEND GLOBAL LANGUAGES memcard MOVIES NIS pfdata SOUND speech SUBTITLES TRACKS root@<vps>:/srv/nfsu2/game/`
   `ssh root@<vps> chmod -R u=rwX,go=rX /srv/nfsu2/game`
   The other languages' voices and movies, if you built them (`LANGPACKS`), the same
   way: `rsync -rlt LANGPACKS root@<vps>:/srv/nfsu2/game/`, then the `chmod` again.
   Before you copy any packs (`LANGPACKS`, `VINYLPACKS`, `MODPACKS`, `TEXPACKS`), check them as the game reads
   them, from the repository: `python3 tools/packcheck.py "<your game folder>"`. It names each file that is wrong
   and exits with 1; copy them only when it says `all good`.
4. **TLS**, in one of these ways:
   - Caddy in front: `deploy/Caddyfile`. Simplest: a Let's Encrypt certificate,
     renewed by Caddy.
   - The server itself on port 443 with a certificate: `deploy/nfsu2-web.service`,
     variant B.
   - A Cloudflare Tunnel: `deploy/cloudflared.yml`, and the service's variant D.
5. **The service.**
   `sudo cp /opt/nfsu2-web/deploy/nfsu2-web.service /etc/systemd/system/`
   Pick its `ExecStart` line (A, D or B) and put your name in `--host`, then run:
   `sudo systemctl daemon-reload && sudo systemctl enable --now nfsu2-web`
   Its log: `journalctl -u nfsu2-web -f`.
6. **Play.** Open `https://<your name>/` in Chrome.

**Who can play:** the server has no accounts or passwords. Anyone who can reach
it can play, and can download the game files from it. Your firewall decides who
that is. Behind Cloudflare's proxy, let only Cloudflare's addresses reach 80 and
443, and turn on Authenticated Origin Pulls (SSL/TLS, Origin Server): any
Cloudflare customer connects from those addresses too, so Caddy (its Cloudflare
variant, with `deploy/cloudflare-origin-pull-ca.pem`) or the server (variant B
with `--trusted-proxies cloudflare`) asks them for Cloudflare's certificate.

**SSH:** log in with a key only.


## Optional: serve silent EA Trax songs

The server normally serves the songs from your own game files unchanged. If you
need to stop serving those songs, `tools/silentmusic.py` creates a **separate**
copy of `SDATA/sdat.viv` with only the song data zeroed. It does not modify or
install the original. In-game speech remains available; players who imported
their own music through **Game → Your copy of the game** can still hear it.

On the machine with your own game files, from the repository root:

```sh
python3 tools/silentmusic.py "<game folder>" /tmp/silent
```

The steps below assume the game files are served from `/srv/nfsu2/game` as
in step 3 above, and that the generated file is already on the server. If
necessary, copy `/tmp/silent/SDATA/sdat.viv` to the server first. **Back up
before replacing anything**, and keep the original outside the served game
directory:

```sh
sudo mkdir -p /srv/nfsu2
sudo mv /srv/nfsu2/game/SDATA/sdat.viv /srv/nfsu2/sdat.viv.songs
sudo cp /tmp/silent/SDATA/sdat.viv /srv/nfsu2/game/SDATA/sdat.viv
sudo chmod 644 /srv/nfsu2/game/SDATA/sdat.viv
curl 'http://127.0.0.1:8080/data-manifest?refresh'
```

Adjust the server address and file ownership/permissions for your setup. If
`SDATA/sdat.viv` is not present in the served game directory, check which
music layout your deployment uses before proceeding. To undo, move the saved
original back to `/srv/nfsu2/game/SDATA/sdat.viv` and refresh the manifest
again. The upstream author reports a successful quick-race browser check with
the silent file; that test has not been repeated for this adapted patch set.

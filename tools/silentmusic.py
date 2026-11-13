#!/usr/bin/env python3
"""The game's music made silent: a copy of the file the EA Trax songs are in with the songs' bytes zeroed, for a
server that must stop sending the songs.  The game plays a "song" as before and nothing is heard; the speech, the
map of the songs and every size and offset stay as they are, so no game code changes.  Players who put their own
copy of the game's music into their browser (the start page's "Your copy of the game") still hear theirs.

    tools/silentmusic.py <game folder> <out folder>

A game folder keeps the songs one of two ways, and the silent copy is written the same way under <out folder>:
  - SDATA/sdat.viv (the PC release's BIG4 archive: the speech and pfdata/MusicSFx.mus and .mpf inside it):
    <out folder>/SDATA/sdat.viv, the archive with the bytes of its pfdata/MusicSFx.mus zeroed;
  - pfdata/MusicSFx.mus (unpacked): <out folder>/pfdata/MusicSFx.mus, all zeros, as long as the original.
To switch a server over, put the file in its game folder in place of the original (keep the original elsewhere to
switch back) and have the server read its folder again (/data-manifest?refresh).  The file is new to every browser
(its date changed), so each downloads it again: it compresses to almost nothing.
"""
import os, struct, sys

CHUNK = 4 << 20

def find(folder, *parts):
    """folder/parts with each part matched whatever its case; None when there is none"""
    cur = folder
    for part in parts:
        hit = next((n for n in os.listdir(cur) if n.lower() == part.lower()), None) if os.path.isdir(cur) else None
        if hit is None:
            return None
        cur = os.path.join(cur, hit)
    return cur

def viv_entries(f):
    """{name in lower case: (offset, size)} of a BIG4/BIGF archive"""
    head = f.read(16)
    if len(head) < 16 or head[:4] not in (b'BIG4', b'BIGF'):
        raise SystemExit('SDATA/sdat.viv is not a BIG4 archive')
    count, out = struct.unpack('>I', head[8:12])[0], {}
    for _ in range(count):
        off, size = struct.unpack('>II', f.read(8))
        name = b''
        while (c := f.read(1)) not in (b'\0', b''):
            name += c
        out[name.decode('latin1').replace('\\', '/').lower()] = (off, size)
    return out

def copy_zeroed(src, dst, start, end):
    """src copied to dst with the bytes start..end-1 zeroed"""
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    with open(src, 'rb') as i, open(dst + '.part', 'wb') as o:
        pos = 0
        while chunk := i.read(CHUNK):
            a, b = max(start, pos), min(end, pos + len(chunk))
            if a < b:
                chunk = chunk[:a - pos] + bytes(b - a) + chunk[b - pos:]
            o.write(chunk)
            pos += len(chunk)
    os.replace(dst + '.part', dst)

def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    game, out = sys.argv[1], sys.argv[2]
    if os.path.realpath(game) == os.path.realpath(out):
        raise SystemExit('the out folder must not be the game folder: the original would be written over')
    viv, mus = find(game, 'SDATA', 'sdat.viv'), find(game, 'pfdata', 'MusicSFx.mus')
    if viv:
        with open(viv, 'rb') as f:
            entry = viv_entries(f).get('pfdata/musicsfx.mus')
        if not entry:
            raise SystemExit('SDATA/sdat.viv has no pfdata/MusicSFx.mus')
        dst = os.path.join(out, 'SDATA', 'sdat.viv')
        copy_zeroed(viv, dst, entry[0], entry[0] + entry[1])
        print(f'{dst}: the songs ({entry[1] / 1048576:.0f} MB at {entry[0]}) are silent; the speech is as it was')
    elif mus:
        dst = os.path.join(out, 'pfdata', 'MusicSFx.mus')
        copy_zeroed(mus, dst, 0, os.path.getsize(mus))
        print(f'{dst}: the songs ({os.path.getsize(mus) / 1048576:.0f} MB) are silent')
    else:
        raise SystemExit('no SDATA/sdat.viv and no pfdata/MusicSFx.mus in the game folder')

if __name__ == '__main__':
    main()

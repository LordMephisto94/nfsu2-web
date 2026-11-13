// The player's own copy of the game's music, copied from their game folder into this browser (OPFS, datacache.ts
// OWN_MUSIC_DIR) and read by the I/O worker in place of the server's (host/io-worker.ts loadOwnMusic) when a game
// starts with NFSU2_MUSIC = own, which shell.ts sets while a whole copy is here.
//
// A game folder keeps the songs one of two ways (datacache.ts): the PC release's archive SDATA/sdat.viv (a BIG4 file:
// the songs, pfdata/MusicSFx.mus and .mpf, and the speech), or those two files unpacked into pfdata.  The copy is made
// in the way the server's folder has them, since it stands in for the server's files by their paths:
//   - the server has sdat.viv: the player's whole sdat.viv (its speech is the same game's);
//   - the server has pfdata's two files: the player's two, from their pfdata or out of their sdat.viv.
// A mark written last (OK_FILE, with the files' sizes) says the copy is whole, so one that stopped halfway is never
// played.
import { MANIFEST_URL, musicLayout, OWN_MPF, OWN_MUS, OWN_MUSIC_DIR, OWN_VIV, type Manifest } from '../host/datacache';

const OK_FILE = 'ok.json';
/** the songs' file is 126 MB (EA's own) to 510 MB (with a soundtrack mod's); the map a few KB to a few hundred */
const MUS_MIN = 16 << 20, MUS_MAX = 2 ** 31, MPF_MAX = 16 << 20;
/** the archive's directory is 320 bytes for its 10 files */
const VIV_HEAD = 64 << 10;
const VIV_MUS = 'pfdata/musicsfx.mus', VIV_MPF = 'pfdata/musicsfx.mpf';

export interface OwnMusic { bytes: number }
/** what a picked folder has of the music (any of them) */
export interface Found { viv?: File; mus?: File; mpf?: File }
/** why a folder's music was not taken, as a fixed word (own-music-ui.ts says it in the page's language) */
export class OwnMusicError extends Error {
  constructor(public why: 'none' | 'not-music' | 'space' | 'write' | 'server', detail = '') { super(detail || why); }
}

const dir = async (create: boolean) => (await navigator.storage.getDirectory()).getDirectoryHandle(OWN_MUSIC_DIR, { create });
const missing = (e: unknown) => (e as DOMException)?.name === 'NotFoundError';
const lower = (s: string) => s.toLowerCase();

/** The whole copy in this browser, or null: none, or one that never finished. */
export async function ownMusic(): Promise<OwnMusic | null> {
  try {
    const d = await dir(false);
    const ok = JSON.parse(await (await (await d.getFileHandle(OK_FILE)).getFile()).text()) as { files?: Record<string, number> };
    const names = Object.keys(ok.files ?? {});
    if (!names.length) return null;
    let bytes = 0;
    for (const name of names) {
      const size = (await (await d.getFileHandle(name)).getFile()).size;
      if (!size || size !== ok.files![name]) return null;
      bytes += size;
    }
    return { bytes };
  } catch { return null; }
}

/** The copy out of this browser (nothing if there is none).  Throws while a game reads it. */
export async function removeOwnMusic(): Promise<void> {
  try { await (await navigator.storage.getDirectory()).removeEntry(OWN_MUSIC_DIR, { recursive: true }); }
  catch (e) { if (!missing(e)) throw e; }
}

/** the music among a folder's files (paths as picked, any case): a file under its usual folder first, else by name */
export function findMusic(files: readonly { path: string; file: File }[]): Found {
  const pick = (name: string, folder: string) => {
    const hits = files.filter(f => lower(f.path.split('/').pop() ?? '') === lower(name));
    return (hits.find(f => lower(f.path.split('/').slice(-2, -1)[0] ?? '') === folder) ?? hits[0])?.file;
  };
  const out: Found = {}, viv = pick(OWN_VIV, 'sdata'), mus = pick(OWN_MUS, 'pfdata'), mpf = pick(OWN_MPF, 'pfdata');
  if (viv) out.viv = viv;
  if (mus && mpf) { out.mus = mus; out.mpf = mpf; }
  return out;
}

/** the music under a picked folder: the game folder (its SDATA and pfdata inside), or one of those two itself */
export async function musicIn(folder: FileSystemDirectoryHandle): Promise<Found> {
  const kid = async <K extends 'file' | 'directory'>(d: FileSystemDirectoryHandle, name: string, kind: K) => {
    for await (const [n, h] of d.entries()) if (h.kind === kind && lower(n) === lower(name)) return h as K extends 'file' ? FileSystemFileHandle : FileSystemDirectoryHandle;
    return null;
  };
  const out: Found = {};
  for (const d of [await kid(folder, 'sdata', 'directory'), folder]) {
    const viv = d && await kid(d, OWN_VIV, 'file');
    if (viv) { out.viv = await viv.getFile(); break; }
  }
  for (const d of [await kid(folder, 'pfdata', 'directory'), folder]) {
    const mus = d && await kid(d, OWN_MUS, 'file'), mpf = d && await kid(d, OWN_MPF, 'file');
    if (mus && mpf) { out.mus = await mus.getFile(); out.mpf = await mpf.getFile(); break; }
  }
  return out;
}

const magic = async (f: Blob) => new TextDecoder('latin1').decode(await f.slice(0, 4).arrayBuffer());
/** whether the two are the game's music: the songs' file starts with a stream ("SCHl"), the map is PathFinder's
 *  ("PFDx", stored low byte first) */
export async function looksLikeMusic(mus: Blob, mpf: Blob): Promise<boolean> {
  return mus.size >= MUS_MIN && mus.size < MUS_MAX && mpf.size > 0 && mpf.size <= MPF_MAX
    && await magic(mus) === 'SCHl' && await magic(mpf) === 'xDFP';
}

/** The files of a BIG4/BIGF archive by their names in lower case (EA's .viv: "BIG4", its size, then big-endian the
 *  count and the directory's size, and per file its offset, size and name); null: not one, or cut short. */
export async function vivFiles(viv: Blob): Promise<Map<string, Blob> | null> {
  const head = new Uint8Array(await viv.slice(0, VIV_HEAD).arrayBuffer()), v = new DataView(head.buffer);
  const tag = new TextDecoder('latin1').decode(head.subarray(0, 4));
  if (head.length < 16 || tag !== 'BIG4' && tag !== 'BIGF') return null;
  const n = v.getUint32(8), out = new Map<string, Blob>();
  let at = 16;
  for (let i = 0; i < n; i++) {
    if (at + 9 > head.length) return null;
    const off = v.getUint32(at), size = v.getUint32(at + 4);
    let end = at + 8;
    while (end < head.length && head[end]) end++;
    if (end >= head.length || off + size > viv.size) return null;
    out.set(lower(new TextDecoder('latin1').decode(head.subarray(at + 8, end)).replace(/\\/g, '/')), viv.slice(off, off + size));
    at = end + 1;
  }
  return out;
}

/** What to copy of a folder's music for a server that keeps the songs as `layout`: the files by the names they get
 *  here.  Throws 'none' when the folder has nothing for it, 'not-music' when what it has is not the game's music. */
export async function musicToCopy(found: Found, layout: 'viv' | 'loose'): Promise<[name: string, data: Blob][]> {
  const inViv = found.viv ? await vivFiles(found.viv) : null;
  if (found.viv && !inViv && !found.mus) throw new OwnMusicError('not-music');
  const vm = inViv?.get(VIV_MUS), vp = inViv?.get(VIV_MPF);
  if (layout === 'viv') {
    if (!found.viv) throw new OwnMusicError('none');
    if (!vm || !vp || !await looksLikeMusic(vm, vp)) throw new OwnMusicError('not-music');
    return [[OWN_VIV, found.viv]];
  }
  if (found.mus && found.mpf) {
    if (!await looksLikeMusic(found.mus, found.mpf)) throw new OwnMusicError('not-music');
    return [[OWN_MUS, found.mus], [OWN_MPF, found.mpf]];
  }
  if (!found.viv) throw new OwnMusicError('none');
  if (!vm || !vp || !await looksLikeMusic(vm, vp)) throw new OwnMusicError('not-music');
  return [[OWN_MUS, vm], [OWN_MPF, vp]];
}

/** how this server's game folder keeps the songs (its manifest); throws 'server' when it does not answer or has none */
export async function serverLayout(fetcher: typeof fetch = fetch): Promise<'viv' | 'loose'> {
  let layout: 'viv' | 'loose' | null = null;
  try {
    const r = await fetcher(MANIFEST_URL, { cache: 'no-cache' });
    if (r.ok) layout = musicLayout((await r.json() as Manifest).files.map(f => f.p));
  } catch { /* said below */ }
  if (!layout) throw new OwnMusicError('server');
  return layout;
}

/** The files copied into this browser, over a copy from before; progress: bytes written of the total. */
export async function installOwnMusic(files: [name: string, data: Blob][], progress?: (done: number, total: number) => void): Promise<OwnMusic> {
  const total = files.reduce((a, [, f]) => a + f.size, 0);
  const est = await navigator.storage.estimate?.().catch(() => null);
  if (est?.quota && est.usage !== undefined && est.quota - est.usage < total + (64 << 20)) throw new OwnMusicError('space');
  try {
    await removeOwnMusic();
    const d = await dir(true);
    let done = 0;
    for (const [name, f] of files) {
      const w = await (await d.getFileHandle(name, { create: true })).createWritable();
      try {
        const r = f.stream().getReader();
        for (;;) {
          const { done: end, value } = await r.read();
          if (end) break;
          await w.write(value);
          progress?.(done += value.byteLength, total);
        }
        await w.close();
      } catch (e) { await w.abort().catch(() => {}); throw e; }
    }
    const ok = await (await d.getFileHandle(OK_FILE, { create: true })).createWritable();
    await ok.write(JSON.stringify({ files: Object.fromEntries(files.map(([name, f]) => [name, f.size])) }));
    await ok.close();
  } catch (e) {
    await removeOwnMusic().catch(() => {});
    throw new OwnMusicError((e as DOMException)?.name === 'QuotaExceededError' ? 'space' : 'write', String(e));
  }
  return { bytes: total };
}

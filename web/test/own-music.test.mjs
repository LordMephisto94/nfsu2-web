// The player's own copy of the game's music (web/src/app/own-music.ts, host/datacache.ts ownMusicName, musicLayout):
// - the music is found in a picked folder's list whatever the case, a file in its usual folder first
// - what is copied is in the layout the server keeps the songs in: the whole sdat.viv, or the two files (the
//   folder's own, or out of its sdat.viv); only the game's music is taken
// - the I/O worker stands the copy in for SDATA/sdat.viv or pfdata's two files and nothing else
import { test } from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import * as esbuild from 'esbuild';
const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const load = async (f) => import('data:text/javascript;base64,' + Buffer.from((await esbuild.build({
  entryPoints: [path.join(ROOT, f)], bundle: true, write: false, format: 'esm', platform: 'neutral', logLevel: 'silent' })).outputFiles[0].text).toString('base64'));
const M = await load('web/src/app/own-music.ts'), D = await load('web/src/host/datacache.ts');
const MB = 1 << 20;

/** a file that starts with head and says it is size bytes long (its slices are of the 64 real bytes) */
const file = (head, size) => { const b = new Uint8Array(64); b.set(Buffer.from(head, 'latin1')); const f = new File([b], 'x'); Object.defineProperty(f, 'size', { value: size }); return f; };
/** a BIG4 archive of the entries [name, bytes] */
function viv(entries, tag = 'BIG4') {
  const dirSize = 16 + entries.reduce((a, [n]) => a + 8 + n.length + 1, 0), start = Math.ceil(dirSize / 64) * 64;
  let at = start;
  const places = entries.map(([, d]) => { const o = at; at += Math.ceil(d.length / 64) * 64; return o; });
  const out = Buffer.alloc(at);
  out.write(tag, 0, 'latin1'); out.writeUInt32LE(at, 4); out.writeUInt32BE(entries.length, 8); out.writeUInt32BE(dirSize, 12);
  let p = 16;
  entries.forEach(([n, d], i) => { out.writeUInt32BE(places[i], p); out.writeUInt32BE(d.length, p + 4); out.write(n + '\0', p + 8, 'latin1'); p += 8 + n.length + 1; Buffer.from(d).copy(out, places[i]); });
  return new File([out], 'sdat.viv');
}
const songs = Buffer.concat([Buffer.from('SCHl'), Buffer.alloc(17 * MB)]), map = Buffer.concat([Buffer.from('xDFP\x04'), Buffer.alloc(100)]);

test('the music is found by name, any case, a file in its usual folder before another', () => {
  const mus = file('SCHl', 1), mpf = file('xDFP', 1), other = file('SCHl', 2), v = file('BIG4', 3);
  assert.deepEqual(M.findMusic([{ path: 'Game/readme.txt', file: other }]), {});
  assert.deepEqual(M.findMusic([{ path: 'Game/PFData/musicsfx.MUS', file: mus }]), {}, 'the two files: both or neither');
  assert.deepEqual(M.findMusic([{ path: 'Game/backup/MusicSFx.mus', file: other }, { path: 'Game/PFData/musicsfx.MUS', file: mus }, { path: 'Game/pfdata/MUSICSFX.mpf', file: mpf }]), { mus, mpf });
  assert.deepEqual(M.findMusic([{ path: 'Game/old/sdat.viv', file: other }, { path: 'Game/SDATA/SDAT.VIV', file: v }]), { viv: v });
  assert.deepEqual(M.findMusic([{ path: 'sdat.viv', file: v }]), { viv: v }, 'SDATA itself picked');
});

test('an archive\'s files by name; what is no archive, or is cut short, is none', async () => {
  const v = viv([['speech/a.big', Buffer.from('SCHlxx')], ['pfdata\\MusicSFx.mus', songs], ['pfdata/MusicSFx.mpf', map]]);
  const files = await M.vivFiles(v);
  assert.deepEqual([...files.keys()], ['speech/a.big', 'pfdata/musicsfx.mus', 'pfdata/musicsfx.mpf']);
  assert.equal(files.get('pfdata/musicsfx.mus').size, songs.length);
  assert.equal(Buffer.from(await files.get('pfdata/musicsfx.mpf').arrayBuffer()).subarray(0, 4).toString('latin1'), 'xDFP');
  assert.equal(await M.vivFiles(new File([Buffer.from('RIFFxxxxxxxxxxxxxxxx')], 'x')), null);
  assert.equal(await M.vivFiles(v.slice(0, v.size - 100)), null, 'a file past the end');
  assert.ok(await M.vivFiles(viv([['a', Buffer.from('1')]], 'BIGF')));
});

test('what is copied follows the server\'s layout; only the game\'s music is taken', async () => {
  const v = viv([['speech/a.big', Buffer.from('SCHlxx')], ['pfdata/MusicSFx.mus', songs], ['pfdata/MusicSFx.mpf', map]]);
  const mus = new File([songs], 'MusicSFx.mus'), mpf = new File([map], 'MusicSFx.mpf');
  const names = async (found, layout) => (await M.musicToCopy(found, layout)).map(([n, f]) => [n, f.size]);
  const why = (found, layout) => M.musicToCopy(found, layout).then(() => 'taken', e => e.why);
  // a server with the archive: the folder's whole archive
  assert.deepEqual(await names({ viv: v }, 'viv'), [['sdat.viv', v.size]]);
  assert.equal(await why({ mus, mpf }, 'viv'), 'none', 'the two files alone make no archive');
  // a server with the two files: the folder's own, or the archive's
  assert.deepEqual(await names({ mus, mpf }, 'loose'), [['MusicSFx.mus', songs.length], ['MusicSFx.mpf', map.length]]);
  assert.deepEqual(await names({ viv: v }, 'loose'), [['MusicSFx.mus', songs.length], ['MusicSFx.mpf', map.length]]);
  assert.equal(await why({}, 'loose'), 'none');
  assert.equal(await why({}, 'viv'), 'none');
  // not the game's music
  assert.equal(await why({ viv: viv([['speech/a.big', Buffer.from('SCHl')]]) }, 'viv'), 'not-music', 'an archive without the songs');
  assert.equal(await why({ viv: new File([Buffer.alloc(64)], 'sdat.viv') }, 'viv'), 'not-music');
  assert.equal(await why({ mus: new File([Buffer.concat([Buffer.from('RIFF'), Buffer.alloc(17 * MB)])], 'm'), mpf }, 'loose'), 'not-music');
  assert.equal(await why({ mus: new File([Buffer.from('SCHl')], 'm'), mpf }, 'loose'), 'not-music', 'too small for the songs');
  assert.equal(await why({ mus, mpf: new File([Buffer.from('ID3')], 'm') }, 'loose'), 'not-music');
});

test('the copy stands in for SDATA/sdat.viv or pfdata\'s two files and nothing else; a folder\'s layout', () => {
  assert.equal(D.ownMusicName('SDATA/sdat.viv'), 'sdat.viv');
  assert.equal(D.ownMusicName('sdata/SDAT.VIV'), 'sdat.viv');
  assert.equal(D.ownMusicName('pfdata/MusicSFx.mus'), 'MusicSFx.mus');
  assert.equal(D.ownMusicName('PFDATA/MUSICSFX.MPF'), 'MusicSFx.mpf');
  assert.equal(D.ownMusicName('pfdata/original/MusicSFx.mus'), null);
  assert.equal(D.ownMusicName('pfdata/sdat.viv'), null);
  assert.equal(D.ownMusicName('sound/MusicSFx.mus'), null);
  assert.equal(D.musicLayout(['SPEED2.EXE', 'SDATA/sdat.viv']), 'viv');
  assert.equal(D.musicLayout(['pfdata/MusicSFx.mus', 'pfdata/MusicSFx.mpf', 'speech/x']), 'loose');
  assert.equal(D.musicLayout(['pfdata/MusicSFx.mus']), null);
  assert.equal(D.musicLayout(['SPEED2.EXE']), null);
});

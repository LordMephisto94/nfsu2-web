// The Game tab's "Your copy of the game" row, under the songs' setting: the player picks the folder their own game is
// installed in, its two music files are copied into this browser (own-music.ts) and every game from then on plays
// them in place of the server's songs; Remove takes them out again.  Under it, a link to the official playlist.
import { findMusic, installOwnMusic, musicIn, musicToCopy, ownMusic, OwnMusicError, removeOwnMusic, serverLayout, type Found, type OwnMusic } from './own-music';
import { t } from './i18n';

/** EA's own playlist of the game's soundtrack */
export const SPOTIFY_PLAYLIST = 'https://open.spotify.com/playlist/2KYnOjOMzSCeQs97ZJiMe1';

export interface OwnMusicRowDeps {
  /** a game runs (or another tab's): its files are in use */
  busy(): boolean;
  log(line: string): void;
}

/** the music of the folder the player picks (nothing in it: none there); null when they cancel */
async function pickGameMusic(): Promise<Found | null> {
  const pick = (globalThis as { showDirectoryPicker?: (o: { mode: 'read' }) => Promise<FileSystemDirectoryHandle> }).showDirectoryPicker;
  if (pick) {
    try { return await musicIn(await pick.call(globalThis, { mode: 'read' })); }
    catch (e) { if (e instanceof DOMException && e.name === 'AbortError') return null; throw e; }
  }
  return new Promise(resolve => {
    // (in the page while it is open: hidden, taken out once answered)
    const input = Object.assign(document.createElement('input'), { type: 'file', webkitdirectory: true, multiple: true, hidden: true });
    const done = (found: Found | null) => { input.remove(); resolve(found); };
    input.addEventListener('cancel', () => done(null));
    input.addEventListener('change', () => done(findMusic(Array.from(input.files ?? [], file => ({ path: file.webkitRelativePath || file.name, file })))));
    document.body.append(input);
    input.click();
  });
}

export class OwnMusicRow {
  readonly el = Object.assign(document.createElement('div'), { className: 'set own-music' });
  private have: OwnMusic | null = null;
  private copying: number | null = null;       // percent, while a copy runs
  private state: { text: () => string; cls: 'ok' | 'bad' } | null = null;

  constructor(private deps: OwnMusicRowDeps) {
    this.el.dataset.key = 'own_music';
    void this.refresh();
  }

  async refresh() { this.have = await ownMusic(); this.render(); }

  private fail(e: unknown) {
    const why = e instanceof OwnMusicError ? e.why : 'write';
    this.deps.log(`[app] music: your own copy was not taken: ${e instanceof Error ? e.message : e}`);
    this.state = { cls: 'bad', text: why === 'none' ? () => t('No music found there. Pick the folder the game is installed in.')
      : why === 'not-music' ? () => t('Those are not the game\'s music files.')
      : why === 'space' ? () => t('Not enough room in this browser for the songs.')
      : () => t('Copying the songs stopped. Try again.') };
  }

  private async choose() {
    if (this.deps.busy()) { this.state = { cls: 'bad', text: () => t('Quit the game first.') }; this.render(); return; }
    this.state = null;
    try {
      const found = await pickGameMusic();
      if (!found) return;
      // copied the way this server's folder keeps the songs (own-music.ts)
      const files = await musicToCopy(found, await serverLayout());
      this.copying = 0; this.render();
      let shown = 0;
      this.have = await installOwnMusic(files, (done, total) => {
        const pc = Math.floor(100 * done / total);
        if (pc !== shown) { shown = this.copying = pc; this.render(); }
      });
      this.deps.log(`[app] music: your own copy of the game's songs is in this browser (${(this.have.bytes / 1048576).toFixed(0)} MB)`);
    } catch (e) { this.fail(e); this.have = await ownMusic(); }
    finally { this.copying = null; this.render(); }
  }

  private async remove() {
    if (this.deps.busy()) { this.state = { cls: 'bad', text: () => t('Quit the game first.') }; this.render(); return; }
    try { await removeOwnMusic(); this.state = null; this.deps.log('[app] music: your own copy was removed'); }
    catch (e) { this.fail(e); }
    await this.refresh();
  }

  render() {
    const label = Object.assign(document.createElement('div'), { className: 'set-label', textContent: t('Your copy of the game') });
    const help = Object.assign(document.createElement('p'), { className: 'set-help', id: 'set-own-music-help',
      textContent: t('Plays the songs from your own copy of the game instead of this server\'s. Pick the folder it is installed in: the songs are copied into this browser.') });
    const ctl = Object.assign(document.createElement('div'), { className: 'set-control' });
    const busy = this.copying !== null;
    const choose = Object.assign(document.createElement('button'), { type: 'button', className: 'btn', textContent: t('Choose game folder'), disabled: busy });
    choose.setAttribute('aria-describedby', 'set-own-music-help set-own-music-state');
    choose.onclick = () => void this.choose();
    ctl.append(choose);
    if (this.have && !busy) {
      const rm = Object.assign(document.createElement('button'), { type: 'button', className: 'btn', textContent: t('Remove') });
      rm.onclick = () => void this.remove();
      ctl.append(rm);
    }
    const st = Object.assign(document.createElement('p'), { id: 'set-own-music-state' });
    if (busy) { st.className = 'set-state'; st.textContent = t('Copying the songs: {percent} %', { percent: this.copying! }); }
    else if (this.state) { st.className = `set-state ${this.state.cls}`; st.textContent = this.state.text(); }
    else if (this.have) { st.className = 'set-state ok'; st.textContent = t('Your own songs play ({size} MB in this browser).', { size: Math.round(this.have.bytes / 1048576) }); }
    else st.hidden = true;
    const link = Object.assign(document.createElement('a'), { className: 'set-help', href: SPOTIFY_PLAYLIST, target: '_blank', rel: 'noopener noreferrer',
      textContent: t('The official playlist on Spotify') });
    this.el.replaceChildren(label, help, ctl, st, link);
  }
}

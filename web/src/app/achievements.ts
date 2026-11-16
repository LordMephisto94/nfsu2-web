// Achievements: earned in the races the player finishes and in the career, kept with the saves (achievements.json in the save folder, so
// a backup and the cloud saves carry them) and listed in the start page's Achievements tab.
//
// runtime/raceresult.c publishes each race's result in four status slots once its results screen is up (rr_n counts
// them: a new value is a new race; rr_race, rr_score and rr_more hold it, packed as there).  While a game runs this
// reads them twice a second, folds each new result into the totals (Totals) and unlocks what they reach, with a toast
// over the game.  The career's are from its state as it stands (cr_state, raceresult.c), so a save gives what it has
// done as it loads.  Nothing goes to the server: the totals are the player's own, from their own games.
import type { GameSession } from './game';
import { count, dateLocale, language, live, LOCALE, N, onLanguage, t, tn, type Msg } from './i18n';

export const FILE = 'achievements.json';

/** a race's result as runtime/raceresult.c packs it */
export interface RaceResult {
  track: number; reversed: boolean; type: number; laps: number; place: number; finished: boolean; mode: number;
  car: number; racers: number; score: number; topKmh: number; margin: number; meters: number;
}
/** the types (raceresult.c RR_*) */
export const CIRCUIT = 0, SPRINT = 1, DRAG = 2, DRIFT = 3, STREETX = 4, URL = 5, KNOCKOUT = 6;
/** the modes */
export const QUICK = 0, CAREER = 1, PLAYERS = 2;

const bits = (v: number, at: number, n: number) => Math.floor(v / 2 ** at) % 2 ** n;
export function unpack(race: number, score: number, more: number): RaceResult {
  return {
    track: bits(race, 0, 16), reversed: bits(race, 16, 1) === 1, type: bits(race, 17, 3), laps: bits(race, 20, 5),
    place: bits(race, 25, 3), finished: bits(race, 28, 1) === 1, mode: bits(race, 29, 2), car: bits(race, 31, 7),
    racers: bits(race, 38, 3), score, topKmh: bits(more, 0, 12) / 10, margin: bits(more, 12, 16), meters: bits(more, 28, 20),
  };
}

/** the career as raceresult.c's cr_state has it (0: not in the career) */
export interface CareerState {
  stagesDone: number[]; finalsWon: number[]; outruns: number; prologueOver: boolean; prologueOutruns: number;
  prologueRaces: boolean; magazines: number; dvds: number; difficulty: number;
  /** cr_more's: the stages (2 .. 5) with every race won but their SUV, sponsor and hidden ones; with every SUV race won */
  cleared: number[]; suv: number[];
}
export const HARD = 2;
export function unpackCareer(v: number, more = 0): CareerState | null {
  if (!bits(v, 0, 1)) return null;
  const stages = [1, 2, 3, 4, 5], later = [2, 3, 4, 5], m = bits(more, 0, 1) ? more : 0;
  return {
    stagesDone: stages.filter(s => bits(v, s, 1)), finalsWon: stages.filter(s => bits(v, 26 + s, 1)),
    outruns: stages.reduce((a, s) => a + bits(v, 2 + 4 * s, 4), 0), prologueOver: bits(v, 32, 1) === 1,
    prologueOutruns: bits(v, 33, 4), prologueRaces: bits(v, 37, 1) === 1, magazines: bits(v, 38, 6), dvds: bits(v, 44, 6),
    difficulty: bits(v, 50, 2),
    cleared: later.filter(s => bits(m, s, 1)), suv: later.filter(s => bits(m, 4 + s, 1)),
  };
}

/** what the results add up to (achievements.json's "totals") */
export interface Totals {
  races: number; wins: number; streak: number; bestStreak: number; meters: number; topKmh: number; bestDrift: number;
  closeWin: boolean; playersWins: number;
  typesWon: number[]; tracks: string[]; wonOn: string[]; bothWays: boolean; carsWon: number[];
  winsByCar: Record<string, number>;
  /** the career's (the most any of the player's careers reached): stages complete, outrun wins, the prologue's */
  stagesDone: number; outruns: number; bayview: boolean; noobie: boolean; prologueRaces: boolean;
  magazines: number; dvds: number; cleared: number[]; suv: number[];
  /** the stages whose final the player won on Hard (seen as it was won) */
  hardFinals: number[];
}
export const emptyTotals = (): Totals => ({
  races: 0, wins: 0, streak: 0, bestStreak: 0, meters: 0, topKmh: 0, bestDrift: 0, closeWin: false, playersWins: 0,
  typesWon: [], tracks: [], wonOn: [], bothWays: false, carsWon: [], winsByCar: {},
  stagesDone: 0, outruns: 0, bayview: false, noobie: false, prologueRaces: false, magazines: 0, dvds: 0, cleared: [], suv: [],
  hardFinals: [],
});

/** the career's state into the totals (a new object; t0 itself when nothing grows).  was: the state seen before in
 *  this game (null: none yet, or out of the career since), so a final that is won between the two on Hard counts;
 *  the finals a save had won already do not */
export function addCareer(t0: Totals, c: CareerState, was: CareerState | null): Totals {
  const s = structuredClone(t0);
  s.stagesDone = Math.max(s.stagesDone, c.stagesDone.length);
  s.outruns = Math.max(s.outruns, c.outruns);
  s.bayview ||= c.prologueOver;
  s.noobie ||= c.prologueOutruns > 0;
  s.prologueRaces ||= c.prologueRaces;
  s.magazines = Math.max(s.magazines, c.magazines);
  s.dvds = Math.max(s.dvds, c.dvds);
  for (const st of c.cleared) if (!s.cleared.includes(st)) s.cleared.push(st);
  for (const st of c.suv) if (!s.suv.includes(st)) s.suv.push(st);
  if (was && c.difficulty === HARD)
    for (const st of c.finalsWon) if (!was.finalsWon.includes(st) && !s.hardFinals.includes(st)) s.hardFinals.push(st);
  return JSON.stringify(s) === JSON.stringify(t0) ? t0 : s;
}

/** a result into the totals (a new object) */
export function add(t0: Totals, r: RaceResult): Totals {
  const s = structuredClone(t0), won = r.place === 1;
  s.races++;
  s.meters += r.meters;
  s.topKmh = Math.max(s.topKmh, r.topKmh);
  if (r.type === DRIFT && r.finished) s.bestDrift = Math.max(s.bestDrift, r.score);
  const key = `${r.track}${r.reversed ? 'r' : ''}`, other = `${r.track}${r.reversed ? '' : 'r'}`;
  if (r.finished && !s.tracks.includes(key)) s.tracks.push(key);
  if (!won) { s.streak = 0; return s; }
  s.wins++;
  s.streak++; s.bestStreak = Math.max(s.bestStreak, s.streak);
  if (!s.typesWon.includes(r.type)) s.typesWon.push(r.type);
  // a win by less than a tenth of a second over the next car home (a drift's margin is points)
  if (r.type !== DRIFT && r.racers > 1 && r.margin > 0 && r.margin < 10) s.closeWin = true;
  if (r.mode === PLAYERS) s.playersWins++;
  if (r.car < 127) {
    if (!s.carsWon.includes(r.car)) s.carsWon.push(r.car);
    s.winsByCar[r.car] = (s.winsByCar[r.car] ?? 0) + 1;
  }
  if (!s.wonOn.includes(key)) s.wonOn.push(key);
  if (s.wonOn.includes(other)) s.bothWays = true;
  return s;
}

/** each achievement: its name, its levels (what each needs, in order; one level: no level numbers shown), what a
 *  level asks in the page's language and how far the totals are (a level is reached when have reaches its need) */
export interface Achievement {
  id: string; name: string; levels: number[]; what: (need: number) => string; have: (t: Totals) => number;
}
const ALL_TYPES = [CIRCUIT, SPRINT, DRAG, DRIFT, STREETX, URL];
const n = (x: number) => ({ n: count(x) });                   // (the count in the page's way, 250.000; tn picks by x)
export const ACHIEVEMENTS: Achievement[] = [
  { id: 'wins', name: N('Winner'), levels: [1, 10, 50, 100, 250], have: t => t.wins,
    what: x => tn(x, 'Win a race.', 'Win {n} races.', n(x)) },
  { id: 'streak', name: N('Winning streak'), levels: [3, 5, 10], have: t => t.bestStreak,
    what: x => tn(x, 'Win {n} race in a row.', 'Win {n} races in a row.', n(x)) },
  { id: 'all_types', name: N('All-rounder'), levels: [ALL_TYPES.length], have: t => ALL_TYPES.filter(x => t.typesWon.includes(x)).length,
    what: () => t('Win a circuit, a sprint, a drag, a drift, a Street X and a URL race.') },
  { id: 'both_ways', name: N('Both ways'), levels: [1], have: t => +t.bothWays, what: () => t('Win on the same track in both directions.') },
  { id: 'photo_finish', name: N('Photo finish'), levels: [1], have: t => +t.closeWin,
    what: () => t('Win a race by less than a tenth of a second.') },
  { id: 'drift', name: N('Sideways'), levels: [50000, 100000, 250000, 500000], have: t => t.bestDrift,
    what: x => tn(x, 'Score {n} point in a drift.', 'Score {n} points in a drift.', n(x)) },
  { id: 'speed', name: N('Speed demon'), levels: [250, 300, 350], have: t => Math.floor(t.topKmh),
    what: x => t('Reach {n} km/h in a race.', n(x)) },
  { id: 'km', name: N('Road trip'), levels: [100, 500, 1000, 5000], have: t => Math.floor(t.meters / 1000),
    what: x => t('Race {n} km in all.', n(x)) },
  { id: 'tracks', name: N('Tourist'), levels: [10, 25, 50], have: t => t.tracks.length,
    what: x => tn(x, 'Finish races on {n} different track (each direction counts).', 'Finish races on {n} different tracks (each direction counts).', n(x)) },
  { id: 'cars', name: N('Collector'), levels: [5, 10, 20], have: t => t.carsWon.length,
    what: x => tn(x, 'Win with {n} different car.', 'Win with {n} different cars.', n(x)) },
  { id: 'loyal', name: N('Loyal'), levels: [10, 25, 50], have: t => Math.max(0, ...Object.values(t.winsByCar)),
    what: x => tn(x, 'Win {n} race with the same car.', 'Win {n} races with the same car.', n(x)) },
  { id: 'players', name: N('Bragging rights'), levels: [1, 10, 50], have: t => t.playersWins,
    what: x => tn(x, 'Win a race against other players.', 'Win {n} races against other players.', n(x)) },
  // the career's
  { id: 'bayview', name: N('Welcome to Bayview'), levels: [1], have: t => +t.bayview, what: () => t('Finish the prologue.') },
  { id: 'noobie', name: N('Yo Noobie'), levels: [1], have: t => +t.noobie, what: () => t('Win an outrun in the prologue.') },
  { id: 'bring_back', name: N('Bring My Car Back... Now!'), levels: [1], have: t => +t.prologueRaces,
    what: () => t('Win the prologue\'s three races.') },
  { id: 'career', name: N('Career'), levels: [1, 2, 3, 4, 5], have: t => t.stagesDone,
    what: x => tn(x, 'Complete {n} stage of the career.', 'Complete {n} stages of the career.', n(x)) },
  { id: 'outruns', name: N('Outrun ace'), levels: [3, 10, 20, 30], have: t => t.outruns,
    what: x => tn(x, 'Win {n} outrun in the career.', 'Win {n} outruns in the career.', n(x)) },
  { id: 'hard', name: N('No mercy'), levels: [1, 2, 4], have: t => t.hardFinals.length,
    what: x => tn(x, 'Win a stage\'s final race on Hard.', 'Win the final race of {n} stages on Hard.', n(x)) },
  { id: 'storm', name: N('Riders on the Storm'), levels: [1], have: t => +(t.stagesDone >= 5), what: () => t('Finish the career.') },
  { id: 'full_clear', name: N('Full clear'), levels: [1, 2, 3, 4], have: t => t.cleared.length,
    what: x => tn(x, 'Win every race of a stage from stage 2 on, but its SUV and sponsor races.',
      'Win every race of {n} stages from stage 2 on, but their SUV and sponsor races.', n(x)) },
  { id: 'suv', name: N('SUV king'), levels: [1, 2, 3, 4], have: t => t.suv.length,
    what: x => tn(x, 'Win every SUV race of a stage.', 'Win every SUV race of {n} stages.', n(x)) },
  // (the game's 30 cover shoots: 20 magazines, 10 DVDs)
  { id: 'magazines', name: N('Cover star'), levels: [1, 5, 10, 20], have: t => t.magazines,
    what: x => tn(x, 'Make a magazine cover.', 'Make {n} magazine covers.', n(x)) },
  { id: 'dvds', name: N('On DVD'), levels: [1, 3, 6, 10], have: t => t.dvds,
    what: x => tn(x, 'Make a DVD cover.', 'Make {n} DVD covers.', n(x)) },
];
/** a level's key in achievements.json's "got" ("wins:2": Winner's second level) */
export const levelKey = (a: Achievement, level: number) => `${a.id}:${level}`;
/** the levels reached (0 none) */
export const reached = (a: Achievement, got: Record<string, number>) => a.levels.filter((_, i) => got[levelKey(a, i + 1)]).length;
/** an achievement's name with its level, as a toast says it ("Sideways II"; one level: its name) */
const ROMAN = ['I', 'II', 'III', 'IV', 'V', 'VI', 'VII', 'VIII'];
export const levelName = (a: Achievement, level: number) => a.levels.length > 1 ? `${t(a.name)} ${ROMAN[level - 1]}` : t(a.name);
/** a level's need, short, in the page's language (50K, 1,5 Tsd.) */
const compact = (x: number) =>
  new Intl.NumberFormat(language() === 'english' ? 'en-GB' : LOCALE[language()], { notation: 'compact' }).format(x);
/** every level of every achievement */
export const LEVELS = ACHIEVEMENTS.reduce((s, a) => s + a.levels.length, 0);

/** achievements.json */
export interface Kept { v: 1; totals: Totals; got: Record<string, number> }
export function parse(text: string | null): Kept {
  try {
    const k = JSON.parse(text ?? '') as Partial<Kept>;
    if (k && k.v === 1 && k.totals && k.got) return { v: 1, totals: { ...emptyTotals(), ...k.totals }, got: { ...k.got } };
  } catch { /* none, or not ours: start again */ }
  return { v: 1, totals: emptyTotals(), got: {} };
}
/** the levels the totals reach that the file does not have yet (an earlier list's file, a level added since), at
 *  `when`: a new object, or k itself when there are none */
export function settle(k: Kept, when: number): Kept {
  let got: Record<string, number> | null = null;
  for (const a of ACHIEVEMENTS) {
    const have = a.have(k.totals);
    a.levels.forEach((need, i) => { if (!k.got[levelKey(a, i + 1)] && have >= need) (got ??= { ...k.got })[levelKey(a, i + 1)] = when; });
  }
  return got ? { ...k, got } : k;
}
/** the result in: the new totals, and the levels it reached (at `when`), each achievement's in order */
export function earn(k: Kept, r: RaceResult, when: number): { kept: Kept; got: { a: Achievement; level: number }[] } {
  return reach(k, add(k.totals, r), when);
}
/** the career's state in (was: the one seen before in this game), as earn */
export function earnCareer(k: Kept, c: CareerState, was: CareerState | null, when: number): { kept: Kept; got: { a: Achievement; level: number }[] } {
  return reach(k, addCareer(k.totals, c, was), when);
}
function reach(k: Kept, totals: Totals, when: number): { kept: Kept; got: { a: Achievement; level: number }[] } {
  const got = { ...k.got }, now: { a: Achievement; level: number }[] = [];
  for (const a of ACHIEVEMENTS) {
    const have = a.have(totals);
    a.levels.forEach((need, i) => {
      const key = levelKey(a, i + 1);
      if (!got[key] && have >= need) { got[key] = when; now.push({ a, level: i + 1 }); }
    });
  }
  return { kept: { v: 1, totals, got }, got: now };
}

export interface AchievementsDeps {
  read: () => Promise<string | null>;
  write: (text: string) => Promise<void>;
  /** the card over the game (#ach-pop) */
  pop: HTMLElement;
  log: (line: string) => void;
}

const POP_MS = 4800, POP_GAP_MS = 350;
const TROPHY = '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M7 4h10v5a5 5 0 0 1-10 0V4z"/>' +
  '<path d="M7 6H4v1a3 3 0 0 0 3 3M17 6h3v1a3 3 0 0 1-3 3M12 14v3M8.5 20h7M10 17h4"/></svg>';

export class Achievements {
  private kept: Kept = parse(null);
  private session: GameSession | null = null;
  private timer: ReturnType<typeof setInterval> | undefined;
  private seen = 0;
  private last = '';
  /** the career's state as last seen in this game (cr_state; null: none yet, or out of the career) */
  private career: CareerState | null = null;
  private careerRaw = '';
  private queue: { a: Achievement; level: number }[] = [];
  private popping = false;
  private busy: Promise<void> = Promise.resolve();

  constructor(private root: HTMLElement, private deps: AchievementsDeps) {
    onLanguage(() => this.render());           // (the levels' short numbers and their tips too)
  }

  /** achievements.json again (a restore, the cloud saves), and the list */
  async load() {
    const read = parse(await this.deps.read().catch(() => null));
    this.kept = settle(read, Date.now());
    if (this.kept !== read) await this.deps.write(JSON.stringify(this.kept)).catch(() => {});
    this.render();
  }

  /** a game started: its results until stop() */
  start(s: GameSession) {
    this.session = s; this.seen = 0; this.last = ''; this.career = null; this.careerRaw = '';
    clearInterval(this.timer);
    this.timer = setInterval(() => this.poll(), 500);
  }
  stop() {
    clearInterval(this.timer); this.timer = undefined;
    this.session = null;
  }

  private poll() {
    const st = this.session?.status?.read();
    if (!st) return;
    const cr = st.get('cr_state') ?? 0, more = st.get('cr_more') ?? 0, careerRaw = `${cr} ${more}`;
    if (careerRaw !== this.careerRaw) {
      this.careerRaw = careerRaw;
      const c = unpackCareer(cr, more), was = this.career;
      this.career = c;
      if (c) this.busy = this.busy.then(() => this.takeWith(k => earnCareer(k, c, was, Date.now())));
    }
    const n = st.get('rr_n') ?? 0;
    if (n <= this.seen) return;
    this.seen = n;
    // (the same result again is the same race: a race's time to the millisecond and its way driven do not repeat)
    const raw = `${st.get('rr_race')} ${st.get('rr_score')} ${st.get('rr_more')}`;
    if (raw === this.last) return;
    this.last = raw;
    const r = unpack(st.get('rr_race') ?? 0, st.get('rr_score') ?? 0, st.get('rr_more') ?? 0);
    this.busy = this.busy.then(() => this.take(r));
  }

  private take(r: RaceResult) { return this.takeWith(k => earn(k, r, Date.now())); }
  private async takeWith(f: (k: Kept) => { kept: Kept; got: { a: Achievement; level: number }[] }) {
    // the file as it is now: another device's (the cloud saves) may have come since
    this.kept = parse(await this.deps.read().catch(() => JSON.stringify(this.kept)));
    const { kept, got } = f(this.kept);
    if (kept.totals === this.kept.totals) return;          // (nothing new: the file as it is)
    this.kept = kept;
    for (const { a, level } of got) {
      this.queue.push({ a, level });
      this.deps.log(`[app] achievement: ${levelKey(a, level)}`);
    }
    this.next();
    await this.deps.write(JSON.stringify(kept)).catch(e => this.deps.log(`[app] achievements: not written (${e})`));
    this.render();
  }

  /** the next level reached onto the card over the game: its name and level, what it took, its levels (the new one
   *  popping in); one after another, each POP_MS */
  private next() {
    if (this.popping) return;
    const n = this.queue.shift(), el = this.deps.pop;
    if (!n) return;
    this.popping = true;
    const { a, level } = n, need = a.levels[level - 1];
    el.replaceChildren();
    el.insertAdjacentHTML('beforeend', TROPHY);
    const body = document.createElement('div'), label = document.createElement('span'), name = document.createElement('strong'),
      what = document.createElement('span');
    body.className = 'ach-pop-body'; label.className = 'ach-pop-label'; name.className = 'ach-pop-name'; what.className = 'ach-pop-what';
    label.textContent = t('Achievement unlocked'); name.textContent = levelName(a, level); what.textContent = a.what(need);
    body.append(label, name, what);
    if (a.levels.length > 1) {
      const chips = document.createElement('ol');
      chips.className = 'ach-levels';
      a.levels.forEach((x, i) => {
        const c = document.createElement('li');
        c.className = i < level - 1 ? 'reached' : i === level - 1 ? 'reached new' : '';
        c.textContent = compact(x);
        chips.append(c);
      });
      body.append(chips);
    }
    el.append(body);
    el.hidden = false;
    el.classList.remove('out'); void el.offsetWidth; el.classList.add('in');         // (the animation again for the next)
    setTimeout(() => {
      el.classList.replace('in', 'out');
      setTimeout(() => { el.hidden = true; el.classList.remove('out'); this.popping = false; this.next(); }, POP_GAP_MS);
    }, POP_MS);
  }

  /** the tab: how many levels, then each achievement with the level reached, what the next asks and how far (the
   *  last reached: its date) */
  render() {
    const { got, totals } = this.kept;
    const all = ACHIEVEMENTS.reduce((s, a) => s + reached(a, got), 0);
    const head = this.root.querySelector('[data-ach-count]');
    if (head) live(head, (() => t('{n} of {total} achievements', { n: count(all), total: count(LEVELS) })) as Msg);
    const list = this.root.querySelector('[data-ach-list]');
    if (!list) return;
    list.replaceChildren(...ACHIEVEMENTS.map(a => {
      const li = document.createElement('li'), level = reached(a, got), next = a.levels[level];
      li.className = level ? (next === undefined ? 'ach got done' : 'ach got') : 'ach';
      const name = document.createElement('strong'), what = document.createElement('span'), state = document.createElement('span');
      live(name, () => level ? levelName(a, level) : t(a.name));
      live(what, () => a.what(next ?? a.levels[a.levels.length - 1]));
      state.className = 'ach-state';
      const when = got[levelKey(a, level)];
      live(state, () => next === undefined ? new Date(when).toLocaleDateString(dateLocale())
        : `${count(Math.min(Math.floor(a.have(totals)), next))} / ${count(next)}`);
      li.append(name, what, state);
      if (next !== undefined && next > 1) {
        const bar = document.createElement('progress');
        bar.className = 'ach-bar'; bar.max = next; bar.value = Math.min(a.have(totals), next);
        li.append(bar);
      }
      if (a.levels.length > 1) {
        const chips = document.createElement('ol');
        chips.className = 'ach-levels';
        a.levels.forEach((need, i) => {
          const c = document.createElement('li');
          c.className = i < level ? 'reached' : i === level ? 'next' : '';
          c.textContent = compact(need);
          c.title = `${levelName(a, i + 1)}: ${a.what(need)}`;
          chips.append(c);
        });
        li.append(chips);
      }
      return li;
    }));
  }
}

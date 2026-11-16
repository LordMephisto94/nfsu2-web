// The Mods tab's catalogue: the community mods this port carries, in the tab's sections (KINDS) and in their order,
// the most important first; then our own changes from the original game (OURS, ADDED: a row per group).  Data only:
// the tab draws it (mods-ui.ts, and settings-ui.ts for the rows with a setting).
//
// Translated (i18n.ts N, a literal each: web/tools/i18n-keys.mjs reads only literals): the fixed texts (KINDS, OURS,
// PLACES, ONLINE, USES, LABELS), each entry's line and our changes' group titles.  The mods' names (a mod's, a part's)
// and authors are never translated; an entry's details and parts, and our changes' names, keys and lines, stay
// English, as the changelog's do.
// web/test/mods.test.mjs checks the entries against settings.ts, the repo's files and the runtime's mod credits.

import { N } from './i18n';

/** the tab's sections, in this order */
export type ModKind = 'car' | 'map' | 'graphics' | 'extras' | 'rules' | 'sound';
/** where a row's setting is when the tab has no control for it */
export type Place = 'display' | 'mainMenu';
/** what changes for other players: mine, on your screen; you, sounds and menus only you get */
export type Online = 'mine' | 'you' | 'seen' | 'career' | 'offline';
/** what this port takes from the mod: its files, packed for the browser (data); its code, ported (code); or what it
 *  found out about the game (addresses, values), written anew (findings); or nothing, a mod made for this port in its
 *  own source (original: no link) */
export type Uses = 'original' | 'data' | 'code' | 'findings';

/** setting: a settings.ini key (settings.ts KEYS) and the values that turn the mod on (none: a switch, on); always: no
 *  setting on the page; elsewhere: settings of its own elsewhere on the page (keys); menu: in the game's own menus */
export type ModControl =
  | { setting: string; values?: readonly string[] }
  | { always: true }
  | { elsewhere: Place; keys: readonly string[] }
  | { menu: Place };

/** port: ported the mod to this game, not its author */
export interface Author { name: string; url?: string; port?: true }

export interface Mod {
  id: string;
  /** the row's name: the mod's, or what the row does where it is a part of a mod (mod: that mod's name) */
  name: string;
  mod?: string;
  /** the mod's authors as its page names them */
  authors: readonly Author[];
  /** page: its page (nfsmods.xyz, or the site its author put it on); source: its source code */
  links: { page?: string; source?: string };
  kind: ModKind;
  uses?: Uses;
  control: ModControl;
  /** the id of the entry whose row this one shares (one control): the row's name lists both, its popup has both */
  row?: string;
  /** the row's help, one short line */
  line: string;
  /** the popup: what it does, what stays as in the original */
  details: string;
  online: Online;
  /** false: its setting off is not the original game (Subtitles: none at all, which its details say); else the popup
   *  says "With it off: as in the original game" (a setting's entry only) */
  off?: false;
  /** a mod of several parts (NFSU2 Extra Options): each part, what it does (English, as details) and who sees it */
  parts?: readonly { name: string; text: string; online: Online }[];
  /** the mod's licence, where it states one */
  licence?: 'MIT' | 'GPL-3.0' | 'No Rights Reserved';
  /** its code ported by the mod's own author, in this repo's history (his commits): no licence file of its own */
  ownPort?: true;
  /** about how much the mod's own files add to the download (a shared row's: each its own, so they add up) */
  sizeMB?: number;
  /** another default on phones (device.ts deviceDefaults) */
  phones?: string;
  /** the repo's files that make it */
  files: readonly string[];
  /** not built yet: its files and its setting may not exist */
  pending?: true;
}

export const KINDS: readonly { kind: ModKind; title: string }[] = [
  { kind: 'car', title: N('Cars') },
  { kind: 'map', title: N('Map') },
  { kind: 'graphics', title: N('Graphics') },
  { kind: 'extras', title: N('Extras') },
  { kind: 'rules', title: N('Game rules') },
  { kind: 'sound', title: N('Sound') },
];
/** the tab's choices of every mod at once (settings-ui.ts presetBar): Default each mod setting's default (the page's
 *  pick: the original game, sharper on a computer), Vanilla every mod off (but those whose off is not the original
 *  game: off: false, which keep their default), Modded every mod on but the Game rules', which change a career */
export type Preset = 'default' | 'vanilla' | 'modded';
export const PRESETS: readonly { id: Preset; name: string; line: string }[] = [
  { id: 'default', name: N('Default'), line: N('Our pick: the original game, sharper on a computer.') },
  { id: 'vanilla', name: N('Vanilla'), line: N('Every mod off, the closest to the 2004 game. The parts under Always on and Our changes stay.') },
  { id: 'modded', name: N('Modded'), line: N('Every mod on, except Game rules: those change your career, so you turn them on yourself.') },
];
/** what a preset writes, a setting at a time: null its default (the line out), a value, or nothing (left as it is).
 *  vals: a setting's choices (settings.ts KEYS: undefined for a switch) */
export function presetValues(p: Preset, vals: (key: string) => string[] | undefined): Map<string, string | null> {
  const out = new Map<string, string | null>();
  const by = new Map<string, Mod[]>();
  for (const m of MODS) if ('setting' in m.control) by.set(m.control.setting, [...by.get(m.control.setting) ?? [], m]);
  for (const [key, ms] of by) {
    if (p === 'default') { out.set(key, null); continue; }
    const v = vals(key), on = ms.map(m => ('setting' in m.control && m.control.values) || ['on']);
    if (p === 'vanilla') {
      if (ms.some(m => m.off === false)) out.set(key, null);
      else out.set(key, v ? v.find(x => !on.flat().includes(x)) ?? v[0] : 'off');
    } else if (!ms.some(m => m.kind === 'rules')) {
      out.set(key, v ? on.flat().find(x => on.every(o => o.includes(x))) ?? on[0][0] : 'on');
    }
  }
  return out;
}

/** the tab's last section: our own changes (ADDED) */
export const OURS = { title: N('Our changes') };

/** a popup's fact where the row has no control of its own: where its settings are (the tab's own words for the page's
 *  places, the game's for its menus) */
export const PLACES: Readonly<Record<Place, string>> = {
  display: N('Its settings: Game, Display'),
  mainMenu: N('In the game\'s main menu: Split Screen'),
};

export const ONLINE: Readonly<Record<Online, string>> = {
  mine: N('Only on your screen'),
  you: N('Only for you'),
  seen: N('Other players see it'),
  career: N('Your career only'),
  offline: N('Not online'),
};

export const USES: Readonly<Record<Uses, string>> = {
  original: N('Built for this project'),
  data: N('Its files, packed for the browser'),
  code: N('Its code, ported'),
  findings: N('Its findings, written anew'),
};

export const LABELS = {
  /** a popup's fact where the mod has no setting */
  always: N('Always on'),
  /** the (i) button's name and tooltip: a mod's row's, a group of our changes' */
  about: N('About this mod: {name}'),
  ours: N('Our changes: {name}'),
  page: N('Mod page'),
  source: N('Source'),
  /** a row's help line: by {names} · its line */
  rowBy: N('by {names}'),
  by: N('By {names}'),
  port: N('Ported by {names}'),
  partOf: N('From {mod}'),
  licence: N('Licence: {licence}'),
  size: N('About {n} MB to download'),
  off: N('With it off: as in the original game'),
} as const;

export const MODS: readonly Mod[] = [
  // ---- Cars
  {
    id: 'crash-damage', name: 'Crash Damage', mod: 'Crashing Foreign Cars', authors: [{ name: 'Chrystian Farias (FRS Mods)' }],
    links: { page: 'https://streetsmodz.com.br/mods/underground-2/crashing-foreign-cars' },
    kind: 'car', uses: 'code', ownPort: true, control: { setting: 'crash_damage' },
    line: N('Dents, loose parts and scratches.'),
    details: 'The racers\' cars, yours and the AI\'s, dent where they are hit, scratch, break their lights and lose their bumpers, mirrors, hood, trunk lid and doors, which lie on the road for a while. Only what is seen: the car drives as in the original game. In free roam, other players who have it on see your car\'s damage; with it off you see every car undamaged. The game\'s own repair, at a race\'s start or in the garage, takes it away.',
    online: 'seen', sizeMB: 1, files: ['runtime/damage.c', 'runtime/damagecore.cpp', 'runtime/damage/Dent.h',
      'runtime/damage/Tessellate.h', 'runtime/damage_none.c', 'tools/damagepack.py', 'tools/damage/scratches.tpk'],
  },
  {
    id: 'stance', name: 'Stance', mod: 'Stance Mod', authors: [{ name: 'Archie (AR4I)' }],
    links: { page: 'https://nfsmods.xyz/mod/1687', source: 'https://github.com/ArchieGoldmill/NFSU2_StanceMod' },
    kind: 'car', uses: 'findings', control: { setting: 'stance' },
    line: N('Camber, offset and tire width for each car.'),
    details: 'Each car\'s camber front and rear, how far its wheels stand out and how wide its tires are; only the look changes, the car drives as in the original game. To use it: turn it on, go to the garage\'s Body Shop and pick Stance (after Rims), choose a slider with up and down, move it with left and right, and Back keeps it for that car. In free roam other players see your car\'s stance, unless they set Mods on other cars to Original. Your saves still work in the original game: the stance is kept beside the profile, and stays there while this is off.',
    online: 'seen', files: ['runtime/stance.c', 'runtime/stance.h'],
  },
  // The rims are out for now (with settings.ts's mod_rims and others_mod_parts, settings-ui.ts's rows and
  // runtime/modpacks.c's shop switch); back with their makers' names and link:
  // {
  //   id: 'mod-rims', name: 'Blitz Rims', authors: [],
  //   links: {},
  //   kind: 'shared', uses: 'data', control: { setting: 'mod_rims' },
  //   line: N('New rim brands in the rim shop.'),
  //   details: 'Blitz, a brand of 24 rims, in the rim shop. Everyone\'s game has them, so a friend\'s car with them looks the same with this off. Off, the shop offers only the game\'s own brands; rims already on your car stay. Your saves still work in the original game: there the car has a similar rim of the game\'s in the same size.',
  //   online: 'seen', sizeMB: 1, files: ['runtime/modpacks.c', 'tools/modpacks.py'],
  // },

  // ---- Map
  {
    id: 'open-tunnel', name: 'Open Construction Tunnel', authors: [{ name: 'roman01la' }], links: {},
    kind: 'map', uses: 'original', control: { setting: 'open_tunnel' },
    line: N('Drive through the Construction Tunnel in free roam.'),
    details: 'The tunnel the game keeps closed for construction is open, to drive through in your career\'s free roam. Only your own city changes: friends you meet in free roam should turn it on too, or you drive where their city has a wall. It needs this server to have its pack.',
    online: 'mine', sizeMB: 2, files: ['runtime/fileio.c', 'tools/open_tunnel.py'],
  },
  {
    id: 'open-parking-lots', name: 'Open Parking Lots', authors: [{ name: 'nfsu2.xyz' }], links: {},
    kind: 'map', uses: 'original', control: { setting: 'car_parks' },
    line: N('Drive into the airport\'s parking garage and more parking lots in the city.'),
    details: 'The airport\'s parking garage and more of the city\'s parking lots are open to drive into in your career\'s free roam. Only your own city changes: friends you meet in free roam should turn it on too, or you drive where their city has a wall. It needs this server to have its pack.',
    online: 'mine', sizeMB: 11, files: ['runtime/fileio.c'],
  },
  {
    id: 'ug1-tracks', name: 'Underground 1 Tracks', authors: [{ name: 'RKBDI' }], links: {},
    kind: 'map', uses: 'original', control: { setting: 'ug1_tracks' },
    line: N('Olympic City from the first Underground in free roam (F9, Tracks) and Quick Race.'),
    details: 'The first Underground\'s Olympic City, Downtown and Mission, Highway 1 and Commercial, and the demo\'s two maps, each a map of its own beside the city. Pick one in the menu\'s Tracks while you drive in career free roam; Back to the city brings you back where you left. Friends you meet in free roam need it on to meet you there. Its maps download as you drive them.',
    online: 'mine', sizeMB: 258, files: ['runtime/ug1tracks.c', 'runtime/travel.c', 'runtime/fileio.c', 'runtime/netrace.c', 'runtime/garage.c'],
  },

  // ---- Graphics
  {
    id: 'uhud', name: 'Upscaled HUD', authors: [{ name: 'LunicAura106' }], links: { page: 'https://nfsmods.xyz/mod/4319' },
    kind: 'graphics', uses: 'data', control: { setting: 'ui_textures', values: ['uhud'] },
    line: N('Sharp menus, HUD and maps.'),
    details: 'The menus, the race HUD, the fonts and the car and brand logos, redrawn at four times their size. The game\'s layout stays as it is, and a language with fonts of its own keeps them. Most phones need four times the memory for them.',
    online: 'mine', sizeMB: 5, files: ['tools/texpacks.py', 'runtime/web/texrep_web.c'],
  },
  {
    id: 'detailed-map', name: 'Detailed Map', authors: [{ name: 'JeansBig' }], links: { page: 'https://nfsmods.xyz/mod/4485' },
    kind: 'graphics', uses: 'data', row: 'uhud', control: { setting: 'ui_textures', values: ['map', 'uhud'] },
    line: N('The minimap and the world map redrawn at 2048x2048.'),
    details: 'The minimap\'s, the world map\'s and the race menus\' maps redrawn at 2048x2048, with the missing shortcuts, and shortcuts and off-road in their own colour. With Map only, the menus and the HUD stay the game\'s own.',
    online: 'mine', sizeMB: 20, files: ['tools/texpacks.py', 'runtime/web/texrep_web.c'],
  },
  {
    id: 'hq-vinyls', name: 'HQ Vinyls & Doorlines', mod: 'High Quality Vinyls & Doorlines (1024x Upscaled)',
    authors: [{ name: 'AdrienShadow' }], links: { page: 'https://nfsmods.xyz/mod/8064' },
    kind: 'graphics', uses: 'data', control: { setting: 'hq_vinyls' },
    line: N('Sharp vinyls and doorlines.'),
    details: 'Every vinyl and doorline of the 31 cars redrawn at 1024x1024, so a car\'s paint stays sharp up close, with the game\'s own designs, colours and shine. A car with one of Art Factory 28 to 34 keeps the game\'s look. On a computer only: phones keep the game\'s. Each car\'s pictures download as you see it, about 100 KB.',
    online: 'mine', phones: 'off', files: ['runtime/hqvinyls.c', 'web/src/gpu/hqskin.ts', 'tools/hqvinyls.py'],
  },
  {
    id: 'sharper-textures', name: 'Sharper textures', authors: [{ name: 'nfsu2.xyz' }], links: {},
    kind: 'graphics', uses: 'original', control: { setting: 'sharper_textures' },
    line: N('The game\'s own textures, sharper.'),
    details: 'The roads and pavements, the city\'s buildings, signs, trees and neon, the cars\' neon and the skies at a higher resolution, the game\'s own designs. On a computer only: phones keep the game\'s. More download data and GPU memory are needed.',
    online: 'mine', phones: 'off', sizeMB: 61, files: ['tools/texpacks.py', 'runtime/web/texrep_web.c'],
  },
  {
    id: 'widescreen', name: 'Widescreen Fix', mod: 'Widescreen Fixes Pack', authors: [{ name: 'ThirteenAG' }, { name: 'Aero_' }, { name: 'xan1242' }],
    links: { page: 'https://nfsmods.xyz/mod/3327', source: 'https://github.com/ThirteenAG/WidescreenFixesPack' },
    kind: 'graphics', uses: 'findings', licence: 'MIT',
    control: { elsewhere: 'display', keys: ['hud', 'fov_scale', 'movie_framing', 'cutscene_borders'] },
    line: N('Any screen shape without stretching.'),
    details: 'The game draws for 4:3. Here the 3D view widens to the screen, and the cutscene bars, rain drops, screen tints and markers over the world fit it. The race HUD can sit at the screen\'s edges, where the mod puts it, and the 3D proportions can be corrected.',
    online: 'mine', files: ['runtime/widescreen.c', 'licenses/WidescreenFixesPack.txt'],
  },

  // ---- Extras
  {
    id: 'gps-signals', name: 'Turn signals with the GPS', authors: [{ name: 'nfsu2.xyz' }], links: {},
    kind: 'extras', uses: 'original', control: { setting: 'gps_signals' },
    line: N('Your car signals by itself before a turn on the GPS route.'),
    details: 'While the GPS shows a route in free roam, your car\'s turn signal comes on before each turn on it and goes off after it. Your own signal keys come first; pressing the side that is already on turns it off until that turn is past. Other players see it like your own signals.',
    online: 'seen', files: ['runtime/signals.c'],
  },
  {
    id: 'gps-distance', name: 'GPS Distance', authors: [{ name: 'roman01la', url: 'https://github.com/roman01la' }], links: {},
    kind: 'extras', uses: 'original', control: { setting: 'gps_distance' },
    line: N('Distance to your destination beneath the GPS arrow.'),
    details: 'While the GPS arrow shows the way, the distance to its destination in a straight line, not by road, in metres or feet as Speedometer Units has it. None in races, or while the GPS finds no route.',
    online: 'mine', files: ['runtime/gpsdistance.c', 'runtime/gpsdistance_format.h'],
  },
  {
    id: 'subtitles', name: 'English Subtitles Fixed', authors: [{ name: 'VileStuff' }], links: { page: 'https://nfsmods.xyz/mod/5286' },
    kind: 'extras', uses: 'data', control: { setting: 'subtitles' }, off: false,
    line: N('Complete English subtitles in the movies.'),
    details: 'The game\'s own English lines stop at their first apostrophe. These are whole, with each speaker\'s name. Other languages keep their own; the dubbed French, German, Italian and Spanish movies have none. Off: no subtitles in any language.',
    online: 'mine', files: ['tools/langpacks.py', 'runtime/language.c'],
  },
  {
    id: 'splitscreen', name: 'Split Screen', mod: 'NFSU2-SplitScreen', authors: [{ name: 'mihaits', url: 'https://github.com/mihaits' }, { name: 'BirdSentinel (Zack)', port: true }],
    links: { source: 'https://github.com/mihaits/NFSU2-SplitScreen' },
    kind: 'extras', uses: 'code', licence: 'MIT', control: { menu: 'mainMenu' },
    line: N('2 to 4 players on one screen.'),
    details: 'The main menu\'s item after Quick Race. Each player gets their own view, HUD, minimap and camera, with a controller each, or two on one keyboard. 3 or 4 players need a fast computer.',
    online: 'offline', files: ['runtime/splitscreen.c', 'runtime/splitscreen_layout.h', 'runtime/splitscreen_mem.h', 'runtime/split_minimap.inc',
      'runtime/split_post.inc', 'runtime/split_reflections.inc', 'tools/split_reloc.py', 'licenses/NFSU2-SplitScreen.txt'],
  },
  {
    id: 'extra-options', name: 'Extra Options', mod: 'NFSU2 Extra Options', authors: [{ name: 'ExOpts Team' }],
    links: { page: 'https://nfsmods.xyz/mod/290', source: 'https://github.com/ExOptsTeam/NFSU2ExOpts' },
    kind: 'extras', uses: 'findings', licence: 'GPL-3.0', control: { always: true },
    line: N('Drift camera, high beam, EU cars and more.'),
    details: 'Five of the mod\'s parts, written anew from what it found out about the game. All five are always on, and the rest of the game stays as in the original.',
    parts: [
      { name: 'Drift camera', online: 'mine',
        text: 'The drift races\' camera in every drive, high behind the car: the camera key\'s fifth view (C, a controller\'s Y), after the game\'s four, which keep their order and look.' },
      { name: 'High beam', online: 'seen',
        text: 'Hold G (its key: Controls, Site) or a controller\'s look stick up for brighter, farther headlights until you let go; a tap turns the headlights off or on. Friends in free roam see it.' },
      { name: 'European cars', online: 'seen',
        text: 'The Peugeot 106 and the Vauxhall Corsa, which only the European release sold, in the car lot and on every car select: Quick Race, Customize, LAN and Online. AI racers drive them too.' },
      { name: 'Special vinyls', online: 'seen',
        text: 'The game\'s hidden promo vinyls (Snoop Dogg, Capone, Chingy, The Doors and more) in a Special category after Sponsor: free, open at stages 2 and 3, adding nothing to the visual rating.' },
      { name: 'Quick Race', online: 'offline',
        text: 'Up to 127 laps, and up to 5 opponents in every race but drags, which keep 3; Lap KO no longer fixes them.' },
    ],
    online: 'seen', files: ['runtime/cameras.c', 'runtime/lightkey.c', 'runtime/regional.c', 'runtime/vinylcat.c', 'runtime/quickrace.c'],
  },

  // ---- Game rules
  {
    id: 'hidden-races', name: 'Show Hidden Races', mod: 'Show Hidden Races Mod', authors: [{ name: 'MaxHwoy' }], links: { page: 'https://nfsmods.xyz/mod/1583' },
    kind: 'rules', uses: 'findings', control: { setting: 'show_hidden_races' },
    line: N('The 33 hidden races on the map.'),
    details: 'In the original game a hidden race shows only when you drive near it. On, they are on the map like the others, and the map offers up to 8 races of each kind at once, not 1 to 4. Turned off again, your career is as it was. oldman777\'s script does the same.',
    online: 'career', files: ['runtime/hiddenraces.c'],
  },
  {
    id: 'rubberbanding', name: 'Rubberbanding', authors: [{ name: '007hacky007', url: 'https://github.com/007hacky007' }], links: {},
    kind: 'rules', uses: 'original', control: { setting: 'catchup', values: ['noboost', 'off'] }, off: false,
    line: N('Career rivals that wait and catch up.'),
    details: 'In a career race the game slows the opponents ahead of you and gives the ones behind more power and grip. Original, the default, keeps that; No boost keeps the waiting, but one behind never beats its own pace; Off drops both, as Quick Race with Catch Up off. Only in your own career races: never in Quick Race, LAN or online races.',
    online: 'career', files: ['runtime/catchup.c'],
  },
  {
    id: 'visual-rep', name: 'Visual Reputation Remover', authors: [{ name: 'thelink2012', url: 'https://github.com/thelink2012' }],
    links: { page: 'https://nfsmods.xyz/mod/2773', source: 'https://github.com/thelink2012/novisualrep' },
    kind: 'rules', uses: 'findings', licence: 'MIT', control: { setting: 'visual_rep_remover' },
    line: N('DVD covers by stage, not by your car\'s parts.'),
    details: 'In the original game a stage\'s DVD cover shoots come only once your car\'s parts rate enough stars, and pay more for more stars. On, the rating is the stage\'s: 0, 1, 3, 5.5 and 8 stars in stages 0 to 4, 10 from stage 5, so the stage\'s cover shoots come and pay by it. The shops show no Visual Rating box.',
    online: 'career', files: ['runtime/visualrep.c', 'licenses/novisualrep.txt'],
  },

  // ---- Sound
  {
    id: 'gearbox-whine', name: 'Transmission Whine', mod: 'Underground 2 Transmission Whine',
    authors: [{ name: 'ropi', url: 'https://github.com/ropi336' }],
    links: { page: 'https://nfsmods.xyz/mod/5927', source: 'https://github.com/ropi336/UG2WhineGen' },
    kind: 'sound', uses: 'code', licence: 'No Rights Reserved', control: { setting: 'gearbox_whine' },
    line: N('NFS Carbon\'s gearbox whine on your car.'),
    details: 'The mod\'s sounds are NFS Carbon\'s: the BMW M3 GTR\'s gearbox whine. Here they play over the game\'s own, a whine rising with the car\'s speed and gear, a second layer louder the faster it goes, a pause at each upshift and a whine of its own in reverse, as loud as the game\'s Master and Engine Volume make them. In split screen and co-op each player\'s car has its own. The sounds download once it is on.',
    online: 'you', sizeMB: 0.5, files: ['runtime/whine.c', 'runtime/whine.h', 'tools/whinepack.py', 'licenses/UG2WhineGen.txt'],
  },
  {
    id: 'keep-engine', name: 'Keep engine sounds', authors: [{ name: 'nfsu2.xyz' }], links: {},
    kind: 'sound', uses: 'original', control: { setting: 'keep_engine' },
    line: N('Engine upgrades no longer swap a car\'s sound.'),
    details: 'In the original game, Street, Pro and Extreme engine upgrades give a car another engine sound. On, every car keeps the engine sound it has with no upgrades: yours, the AI racers\' and other players\' cars as you hear them. Only the sound changes: the upgrades work as before.',
    online: 'you', files: ['runtime/keepengine.c', 'runtime/roam_game.c'],
  },
  {
    id: 'songs', name: 'Uncensored & Restored Soundtrack', authors: [{ name: 'xGMVx' }], links: { page: 'https://nfsmods.xyz/mod/3649' },
    kind: 'sound', uses: 'data', control: { setting: 'songs', values: ['uncensored'] },
    line: N('The EA Trax songs unedited.'),
    details: 'Every EA Trax song in its full, unedited version, some from their newer remastered releases, made with BadHairDay\'s NFS U2 Music Importer. Uncensored, the default, plays them; Original plays the versions the game shipped with.',
    online: 'you', files: ['tools/musicpack.py', 'web/src/host/io-worker.ts'],
  },
  {
    id: 'high-fps-audio', name: 'High FPS Audio Fix', mod: 'NFSU2 High FPS Audio Fix', authors: [{ name: 'dstudnicki' }],
    links: { source: 'https://github.com/dstudnicki/NFSU2HighFPSAudioFix' },
    kind: 'sound', uses: 'findings', control: { always: true },
    line: N('Engine sounds right above 60 fps.'),
    details: 'The game updates its sounds at most every 15 ms, and then by one frame\'s time only: above 60 fps the engine\'s pitch and the gear shifts ran slow, at 120 fps at half speed. Here an update also gets the time of the frames it skipped. At 60 fps and below nothing changes.',
    online: 'you', files: ['runtime/framerate.c'],
  },
];

/** one of our changes: its name, its key ('': none) and what it does, in one short line */
export interface Change { name: string; key: string; line: string }

/** our changes from the original game, in the tab's last section (OURS): a row per group, named by its title and
 *  with a few of its changes' names as its help, and a popup with all of them.  Every change is here, the small ones
 *  too; none has a row of its own, and none is named as a mod's row is (web/test/mods.test.mjs). */
export const ADDED: readonly { id: string; title: string; items: readonly Change[] }[] = [
  { id: 'driving', title: N('Driving'), items: [
    { name: 'Drift mode', key: 'X', line: 'Drift handling, camera and score in free roam' },
    { name: 'Free look', key: 'right stick', line: 'Look round the car; straight down looks back' },
    { name: 'Horn', key: 'N', line: 'Pick your horn; others hear it, in races too' },
    { name: 'Horn Volume', key: '', line: 'Options, Audio: how loud every horn is' },
    { name: 'Radio', key: 'F9 menu', line: 'Your own music folder in place of EA Trax' },
    { name: 'Next song', key: 'F7', line: 'Skip to the next song while you play' },
    { name: 'Music in drag and drift races', key: '', line: 'Songs in drag and drift races too; off at first' },
  ] },
  { id: 'free-roam', title: N('Free roam'), items: [
    { name: 'Free roam with others', key: '', line: 'Your career\'s city with everyone: up to 64 cars' },
    { name: 'Ghost mode', key: 'F9 menu', line: 'Drive through players, or traffic and tuners too' },
    { name: 'Same weather for everyone', key: '', line: 'It rains in the same places for every player' },
    { name: 'Quick chat', key: '1–4', line: 'Phrases for players near you, in their language' },
    { name: 'Crews', key: '', line: 'Up to 4 crews, seen anywhere in their colour' },
    { name: 'Friends', key: 'F', line: 'Touch front bumpers to add a friend; join them' },
    { name: 'Messages to friends', key: 'I', line: 'Write to a friend in free roam or from this page' },
    { name: 'Your name', key: '', line: 'Your name over your car; claim it to keep it' },
    { name: 'Your flag', key: '', line: 'A country flag before your name; none at first' },
    { name: 'Show Player Names', key: '', line: 'Options, Gameplay: names over cars on or off' },
    { name: 'Rooms', key: '', line: 'A room keeps to a group; share it by a link' },
    { name: 'Blocked players', key: '', line: 'Players you block never meet you in free roam' },
    { name: 'Outruns between players', key: 'Enter', line: 'Drive close for 4 s, then challenge a player' },
    { name: 'Drift battles', key: 'X, then Enter', line: 'Most drift points in 90 s wins $100 from the other' },
    { name: 'Races from free roam', key: 'Enter', line: 'Open a lobby at a race\'s start, up to 6 players' },
    { name: 'Cars evened out', key: '', line: 'Lobby races even out the cars\' engine power' },
    { name: 'Prize money and quit fee', key: '', line: 'Lobby races pay by place; quitting costs money' },
    { name: 'Car meets', key: 'M', line: '20+ parked players: a star on the map, GPS to it' },
    { name: 'Players on the maps', key: 'M', line: 'Arrows on the minimap, markers on the world map' },
    { name: 'Other players\' cars', key: '', line: 'Their engines, nitrous, hydraulics and tyre smoke' },
    { name: 'Hood, trunk and doors', key: 'O', line: 'Parked, open them one by one; others see them' },
    { name: 'Player 2 in free roam', key: 'pause menu', line: 'A second player on this computer joins you' },
    { name: 'Online mode', key: '', line: 'The Online menu works again: lobby, chat, rankings' },
    { name: 'Online and LAN races', key: '', line: 'Every track open, and up to 6 players, not 4' },
    { name: 'LAN races with everyone', key: '', line: 'The game\'s LAN menu reaches everyone here' },
    { name: 'Smooth cars in races', key: '', line: 'Players\' cars glide in races, as in free roam' },
  ] },
  { id: 'photos', title: N('Photos and screen'), items: [
    { name: 'Photo mode', key: 'F12', line: 'Freeze the world, move the camera, take a photo' },
    { name: 'Hide HUD', key: 'F10', line: 'Hide the HUD and names over cars while driving' },
    { name: 'Frame rate', key: '', line: 'Auto picks 60, 120 or 240 fps from your screen' },
    { name: 'Frame-rate counter', key: 'F8', line: 'Shows the frame rate: off, on or detailed' },
    { name: 'Exposure and White Level', key: '', line: 'Options, Display: brighter, darker, dimmer whites' },
    { name: 'Other cars\' headlights', key: '', line: 'Players\' cars light the road too, or all cars' },
    { name: 'Pixelated textures', key: '', line: 'Blocky textures, like early PC games; off at first' },
    { name: 'Startup movies', key: '', line: 'Skip the logos, PSA and intro with a key' },
    { name: 'Movies', key: '', line: 'Turn the intro and story movies off' },
  ] },
  { id: 'career', title: N('Career and garage'), items: [
    { name: '10 garage slots and Trade Car', key: '', line: 'Up to 10 cars, not 5; Trade Car sells one' },
    { name: 'Unique parts after Caleb', key: '', line: 'Once Caleb is beaten, unique parts for every car' },
    { name: 'Save in the pause menu', key: 'Esc', line: 'Free roam saves your career where you stand' },
    { name: 'Career wins with players', key: '', line: 'A lobby win (or a friend\'s) counts in your career' },
    { name: 'The Bank when it changes', key: '', line: 'Free roam\'s Bank shows only after it changes' },
    { name: 'Power Units', key: '', line: 'Options, Gameplay: engine power in bhp or kW' },
    { name: 'Turn the car in the garage', key: 'right stick', line: 'The right stick or two fingers turn it' },
    { name: 'Tuning graphs', key: '', line: 'Drag the ECU and Turbo graphs\' dots' },
    { name: 'Art Factory 28 to 34', key: '', line: 'The Japanese release\'s 7 vinyls, on every car' },
  ] },
  { id: 'controls', title: N('Controls'), items: [
    { name: 'Site keys', key: '', line: 'Set or turn off this site\'s keys and buttons' },
    { name: 'Menu', key: 'F9', line: 'Full screen, ghost mode, crews, radio and more' },
    { name: 'Controllers', key: '', line: 'Xbox and PlayStation pads, with rumble' },
    { name: 'Steering wheels', key: '', line: 'Logitech and other wheels, with shifters' },
    { name: 'RC radios', key: '', line: 'A model transmitter on USB drives as a wheel' },
    { name: 'Touch controls', key: '', line: 'Drive and use the menus on a touch screen' },
  ] },
  { id: 'saves', title: N('Saves'), items: [
    { name: 'Cloud saves', key: '', line: 'Your saves in any browser, under a passphrase' },
    { name: 'Backup and restore', key: '', line: 'One zip of your saves; PC game profiles too' },
    { name: 'Screenshots', key: 'F12', line: 'View, download or delete them in Saves' },
    { name: 'Play time', key: '', line: 'How long you have played in this browser' },
    { name: 'Autosave', key: '', line: 'Profiles autosave, and "Progress saved" says so' },
    { name: 'Rename', key: '', line: 'Give a profile another name, its career with it' },
    { name: 'First profile\'s name', key: '', line: 'Your first profile starts as Your name' },
  ] },
];

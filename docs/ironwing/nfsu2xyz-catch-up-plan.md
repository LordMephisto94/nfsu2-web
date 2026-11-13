# Catching up with nfsu2.xyz: what changed and how IronWing can do the same

Written 13 November 2026 from nfsu2.xyz's changelog (/changelog.json: the releases of 9, 10, 11 and 12 November) and
its Discord bug reports. The original project went private on 9 November, so none of this code is public: each item
below has to be rebuilt here from its description, and will behave like nfsu2.xyz's, not identically.

Effort: **S** = hours, **M** = about a day, **L** = several days, **XL** = a week or more. "Repro first" means the
changelog says what was fixed but not why: the bug has to be found again before it can be fixed.

## Phase 0: legal and safety (first)

| What | Why | How | Effort |
|---|---|---|---|
| **Music: keep the EA Trax songs, add players' own** | IronWing keeps serving the songs (the user's call, 13 Nov). nfsu2.xyz made theirs silent on 10 Nov, very likely against DMCA notices from the record labels, so a quick way to do the same stays ready | 1. Start page Music dialog: **Your copy of the game** (pick the game folder; the page finds the songs in `pfdata` and keeps them in the browser, `NFSU2_MUSIC=own`), **Your music** (the existing Radio, with the Phase 2 Radio items) and a link to the official Spotify playlist. 2. Ready but off: a tool that writes a silent copy of each song in `pfdata` (same names and container, so the game still plays a "song"), to install on the LXC within minutes if a notice ever arrives; players with their own copy keep their music then. nfsu2.xyz's code shows this design: `NFSU2_MUSIC=own/silent` | L |
| **Crash Damage** | Freezes when cars hit | Already off on IronWing (10 Nov). Fix later (Phase 3) | done |
| **Cloud saves: an empty or broken browser no longer replaces the saves in the cloud** | Players can lose their saves | Repro first in `vault.ts`: refuse an upload that would replace a cloud copy with an empty or unreadable set | M |

Phase 0 status (13 Nov, branch `catch-up`): the cloud-saves fix is in (a restore that stopped halfway is no longer
sent as the player's choice). The music is in: the Game tab's **Your copy of the game** row (IronWing's game folder
keeps the songs in `SDATA/sdat.viv`, so the player's whole `sdat.viv` is copied; a server with pfdata's two files gets
those), the Spotify link, and `tools/silentmusic.py` (NFSU2-LXC.md says how to switch it on). Checked in a browser with
`web/tools/own-music-e2e.mjs`; the quick race passes with the silent file.

## Phase 1: fixes for things IronWing already has

Done on `catch-up` (13 Nov): full screen locks only Esc and the browser's risky shortcut keys (other programs' keys
work); a stick at its rim steers fully; no rumble under the page's menu or dialogs; Last.fm titles without quotation
marks; an Outrun game takes four players (the server's lobby). Rumble under the game's own pause menu and after a
race is still open (runtime).

Free roam and races
- Lobby races: the host's game freezing as players join (repro first) **M**
- Lobby races: a race or lobby lost when the server restarts for an update: keep lobby state in the server's state folder **M**
- Lobby races: every player gets a place, the same in every game, finish times match; a car that stands still all race **L**
- Lobby races: a slow racer passes through you instead of ramming; a light touch pushes instead of stopping dead (`racenet.c`) **L**
- Free-roam race joiners sent back to free roam ("the host's game did not answer"; open in the bug reports) **L**
- Free roam: race and event markers vanishing among many players (open in the bug reports) **M**
- Free roam: other players' upgraded engines under an open hood, and a new career's 350Z **M**
- Free roam: freeze after a stage final's movie when you left the crib meanwhile (repro first) **M**
- Free roam: less stutter on slower computers as friends' cars arrive **M**
- Races: nitrous held at the start firing on its own after a restart **S**
- Races: bumper and hood views going through a car that passes through yours (Ghost mode Everything; open in the bug reports) **M**
- Sound: your own car centred in the bumper and hood views **S**
- Drift mode: GPS arrow readable from the drift camera; turning drift mode off with the handbrake held leaving the car revving; HUD missing in drift races **M**
- Handbrake as in the original game, cars staying revving instead of driving off (check whether IronWing's build has this) **M**
- Exhaust pops and smoke come out the way the tip points; IS300 body kit 11 rear bumper's single tip **M**

Online (the EA Online lobby)
- Outrun games with 5 or 6 players freeze the host: cap Outrun at 4 players, as the original game does **S**
- Evenly Matched games take cars of the host's class only, as in the original **S**
- Start Race works again after races that players never came back from **M**
- Invites sent from the game reach the other player; the game room shows each player's own car and class **M**

Career
- Show Hidden Races: every open race has its icon on the world map and minimap; finished careers showing "not completed" (open) **M**
- Car lot: a car missing after a URL race (Skyline): each URL race win opens the stage's next car, in any order **M**
- Event Results ignoring Enter (open in the bug reports) **M**

Controllers and keys
- Controllers: the Joypad Configuration kept when the controller connects after the profile loads (may be part of the Xbox "RT acts as Start" reports) **M**
- Controllers: a stick pushed fully to a side steers fully also when held a little up or down; throttle, brake or steering on the right stick no longer let go after two seconds held still **S**
- Controllers: no rumble under the pause menu, after the race, or with the page's menu open **S**
- Controllers: a shifter or handbrake on its own no longer holds the steering over; bindings survive switching wheel/controller **M**
- Steam Deck's X and Y in Firefox on Linux **S**
- Full screen: other programs' keys (OBS, GeForce Experience, Discord push to talk) work again **S**
- Mouse: a new resolution in the pause menu no longer moves the highlight **S**
- Steering wheels: steer from the centre with no dead zone; Set up takes full lock where the wheel stops; pedals that connect late join the set-up; sequential shifter set-up finishes **M**

Start page and loading
- Play works for players who got "error code 4" every time (repro first) **M**
- Quit brings the start page back after an error at Play; a browser too old says so instead of retrying **S**
- Play makes room when the browser's storage is full, saves kept **M**
- A broken game file: tell the player to use Delete game data instead of error 134; re-download a part the browser kept broken after a hard close **M**
- Sound that fails to start is tried again instead of playing silent **S**
- No dropped frames on computers whose clock changed while the page was starting **S**
- Radio: long folder and artist names fit; Chrome and Edge ask for the music folder again at the first click; a folder with other music plays all of it in turn **S**
- Menus: bigger help bar buttons stay on the dialogs **S**
- Last.fm: titles scrobbled without quotation marks **S**

Photo mode and co-op
- Photo mode: no error 134 after closing it; each side mirror stays with its door; your car in the picture from the bumper view; names for every car the Scene tab shows; Reset resets the Picture tab **M**
- Co-op: no crash after player 2 leaves; Cancel/Back on Add Player 2; player 2's car keeps its paint in a crowd; player 1's headlight key; the outrun icon's sound looping; player 2 counted as Your car in photo mode **M**

Dyno
- The old ECU and turbo glitch no longer gives extra torque, **only in races with other players** (on your own, the original game, as AGENTS.md's product rules require) **S**

## Phase 2: small and medium features

| Feature | How | Effort |
|---|---|---|
| A dot on the Changelog button when there is something new | Remember the newest release seen (`prefs.ts`) | S |
| Online lobby Filter: Reset to default, hide games with a password | `olfilter.c` | S |
| Garage: K and L turn the car | `garage.c` | S |
| F9 menu: Friends and Crews in one row; Mods tab sorted by topic | page only | S |
| Start page: fewer settings rows, Frame rate says what Auto gives, a new browser starts in its own language | page only | M |
| Music keeps playing on the world map | `radio.c` / the game's music state | M |
| Radio: every song as loud as the game's own; .zip, .webm, .aac, .mp4 files | page's audio decode, loudness measured per file | M |
| Radio on iPhone and iPad; Firefox and Safari remember the music | OPFS copy of the chosen files where the File System Access API is missing | M |
| Messages: unread count in free roam, I opens the conversation with your last messages | page overlay + server message history | M |
| Mods: Keep engine sounds (engine upgrades no longer swap the car's sound) | `carsfx.c`, off by default | M |
| Rearview mirror option: All (close and far cameras too) | `cameras.c` | M |
| Stance: Spoiler On/Off, Ride Height slider (60 mm lower to 40 mm higher), Spoiler Off holding in the Body and Paint shops | `stance.c` (exists) | M |
| Car Specialties: Nitrous Flame colour, seen by friends | `specialties.c` + the free-roam car state | M |
| Cover shoots: other players' cars out of the career's shoots (Show Players brings them back) | `roam_game.c` | M |
| Co-op: player 2's headlights (right stick), show-off and photo mode for player 2 | `coop.c` | M |
| Split screen: steering wheels can take a seat | `local-pads.ts` + `wheel.ts` | M |
| Free roam tuning: pick the dyno setup your car drives in career free roam | `dyno.c` + a setting | M |
| Sound: about 45 ms less delay | AudioWorklet buffer sizes, measured | M |
| Controllers: about 6 ms less input lag | when the page polls the pad against the game's frame | M |
| Anti-aliasing at less frame-rate cost; turn itself off where it stops the graphics right after Play | `gpu/` | M |
| About 250 MB less to download: cars' vinyl textures at half size | a data tool for the vinyl packs; served from the LXC game folder | L |
| Frame rate: less work per frame, frames on time in a busy free roam | profiling first | L |
| Sharper roads (Mods, Graphics), off by default | it made busy free roam stutter on nfsu2.xyz | L |
| Hood decals in the Graphics Shop; Art Factory, Unique and Contest vinyls leave the other layers free | the vinyl layers in `vinylcat.c` / `vinylpacks.c` | L |
| Cloud saves: keep the passphrase as a passkey or a QR code | WebAuthn (passkey with the PRF extension) and a QR image on the page | L |

## Phase 3: big features

| Feature | How | Effort |
|---|---|---|
| **Lobby races with AI racers** (host's setting, off by default), a host settings page (who joins, places, AI racers, laps, Ghost mode, wall rides), a top-half place counting in the career, a six-racer leaderboard, engines revving with their throttle, and its follow-up fixes of 11 Nov | `mprace.c` (2,100 lines) and `racenet.c`: the game's own AI cars filling empty LAN slots on the host, their positions sent like a player's | XL |
| **Daily jobs** (three a day, the same for everyone, F9 menu "Today", pays like a multiplayer win, off until started) and **the Daily Run** (shop to shop against the clock) | Server: the day's jobs from the date. Runtime: the timer HUD, the GPS target, crash and speed-trap checks, the payout | XL |
| **Steering wheel force feedback** in Chrome and Edge (Logitech, Fanatec, Thrustmaster, Moza, direct-drive bases) | WebHID, one protocol per maker; the game's DirectInput effects (`dinput_pad.c` already turns them into rumble) mapped to forces | XL |
| Steering wheels: any game control on any button, H-pattern shifter picks the gear, any controller as a wheel | `wheel.ts` + `wheel-setup.ts` | L |
| **Use your phone as a controller** (scan a code, same Wi-Fi) | a phone page with touch controls, linked through the server by a one-time code | L |
| **FPV drone** in free roam (F6) | a free camera with drone physics in `cameras.c`/`photo.c` | XL |
| Photo mode: Car tab (rewind your car up to 10 s, copies, posing), Studio backgrounds, magazine and DVD frames, tilt focus, zoom and spin blur, open over a replay with F12 | `photo.c` (1,450 lines) and the page's photo panel | XL |
| **Construction Tunnel** as a road in career free roam (Mods, Extras), with its 10 and 11 Nov fixes (both sides open, walls that stop a car, races keep the city's construction) | changes to the world's collision and geometry; the hardest kind of change here | XL |
| Missed unique performance parts won back after Caleb (every 3 outrun wins) | changes career rewards: against AGENTS.md's "original game by default" unless it is an off-by-default mod | M |
| Crash Damage fixed (freezes when cars hit), then back on the Mods tab | profile `damage.c` / `damagecore.cpp` on a long free roam | L |
| Anti-cheat: more cheats caught in races and free roam | no details given; the `nfsu2-anticheat` crate | ? |

## Not needed on IronWing

Fixes of features IronWing doesn't have (daily jobs, the drone, the Construction Tunnel, Sharper roads, HQ vinyls'
stutter and paint mix-ups: IronWing has no HQ vinyl pictures), and the 9 November regression ("crash after 2-3
minutes", fixed the same day): they come with their features, if those are built.

## Suggested order

1. Phase 0: the cloud-save safety fix, then the music (players' own music, and the silent-songs switch kept ready).
2. Phase 1, most-reported first: lobby races, free roam, controllers, the start page.
3. Phase 2 in small batches, one deploy each, each with its CHANGELOG.md line "by IronWing Networks".
4. Phase 3 one feature at a time, starting with lobby AI racers or daily jobs, whichever the IronWing community wants
   most.

Phase 1 and 2 are a few weeks of work together; Phase 3 is several weeks more. Controller and wheel work needs real
hardware to test (or a community member with it).

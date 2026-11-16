/* raceresult.c -- each finished race's result, for the page (web/src/app/achievements.ts).
 *
 * Once a race's results screen has been up for RR_SETTLE frames (UI_PostRaceResults.fng, a URL tournament's
 * UI_PostRace_TournResults.fng; its rows may come a little after it), or as it goes if that is sooner, the race is
 * read once and published in four
 * status slots (runtime/web/status_web.c), in one consistent section:
 *
 *   rr_n      the results published since the game started (1, 2 ...): a new value is a new race
 *   rr_race   track number (16 bits) | reversed << 16 | type << 17 (RR_*, 3 bits) | laps << 20 (5) |
 *             place << 25 (3: 0 not finished) | finished << 28 | mode << 29 (2: 0 Quick Race and the rest, 1 career,
 *             2 a race between players) | car type << 31 (7: the car table's index, 127 none) | racers << 38 (3)
 *   rr_score  the time (ms) or, in a drift, the points; 0 not finished
 *   rr_more   top speed (0.1 km/h, 12 bits) | the margin << 12 (16: a win's over 2nd, else behind 1st; centiseconds,
 *             a drift's in points) | the way driven << 28 (m, 20 bits)
 *
 * Each field is a whole number well under 2^53, so a double holds it exactly.  A race left with its pause menu's
 * Quit has no results screen, so no result; an outrun has none either.  Each result is also a [race] line in the log.
 *
 * What it reads (as discord.c and mprace.c read them): the race parameters [0x89E7A0] (track, direction, kind, laps,
 * the racers and which is player 1), the results' rows [0x851044] x [0x85105C] (0x515DB0 fills them for the results
 * screen: +4 the car's id, +0xC its time in the game clock's 1/4000 s, +0xC8 laps done, +0x104 a drift's points,
 * +0x10C top speed and +0x110 average speed, m/s) and player 1's car [[PlayersByNumber] + 4].
 *
 * The career as it stands, for the career's achievements, in one more slot, set when it changes (each 30 frames):
 *
 *   cr_state  0 outside the career (the game mode not 1), else: 1 | stage s complete << s (s 1 .. 5) |
 *             its outrun wins << (2 + 4 s) (4 bits, at most 15) | its final event won << (26 + s) (a stage that
 *             names none, complete by its races as stage 1 is: complete) |
 *             the prologue over (the stage 1 or more) << 32 | the prologue's outrun wins << 33 (4) |
 *             the prologue's three races won (STAGE_0_CIRCUIT_0, _1, STAGE_0_SPRINT_0), only while in it, << 37 |
 *             magazine covers done << 38 (6 bits) | DVD covers done << 44 (6) | the difficulty << 50 (2: 0 Easy ..)
 *
 *   cr_more   0 outside the career, else: 1 | stage s (2 .. 5) fully cleared << s (every one of its races won but its
 *             SUV races (S<s>_SUV_k), its sponsors' (S<s>_SPON_*: only the signed sponsor's can be raced) and its hidden
 *             ones (S<s>_H_*: only the Show hidden races mod opens them)) | every SUV race of stage s won << (4 + s)
 *
 * From the career's own records (game.h: the stages' records, the events' states, the race table, the cover shoots
 * and the difficulty), so a save gives what it has done as it loads.  A cover shoot is done when its event is won;
 * its kind says magazine or DVD.  The game marks the prologue's own events won without a race (career.c), so its
 * three races count only while the career is still in it (the page keeps what it saw).  Logged as "[career] state". */
#include "cpu.h"
#include "game.h"
#include "splitscreen_mem.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

__attribute__((weak)) int host_status_slot(const char *name){ (void)name; return -1; }
__attribute__((weak)) void host_status_set_n(const int *slot, const double *value, int n){ (void)slot; (void)value; (void)n; }
const char *autotest_top_fng(void);
int career_fe_active(const char *name);
long autotest_frame(void);

#define G_RACEP        0x0089e7a0u   /* the race parameters, filled before a track loads */
#define   R_TRACK       0x00u       /*   int  track number */
#define   R_REVERSE     0x04u       /*   int  1: reversed */
#define   R_KIND        0x10u       /*   int  3: lap knockout */
#define   R_P2P         0x14u       /*   int  point to point (sprint, drag) */
#define   R_LAPS        0x1cu
#define   R_DRAG        0x38u       /*   bytes: drag, drift, URL, Street X */
#define   R_DRIFT       0x39u
#define   R_URL         0x41u
#define   R_SHORT       0x43u
#define   R_HUMAN       0x68u       /*   int  player 1's racer index */
#define   R_RACERS      0x70u       /*   racer records, 0x940 bytes each: +0x10 car type */
#define   R_NRACERS     0x74u
#define G_RACEKIND    0x0089e7b0u   /* 0 free roam, 7 an outrun */
#define G_RSTATUS     0x00890118u   /* the race coordinator*: +0 state, 4 racing */
#define G_PLAYER1     SPLIT_PLAYERS_INDEX
#define G_RESULTS     0x00851044u
#define G_RESULTS_N   0x0085105cu
#define RESULTS_MAX   8
#define ROW_SIZE      0x224u
#define RR_SETTLE     5             /* frames on the results screen before they are read (a drift's screen can
                                     * be gone after 27: the player's Continue) */

enum { RR_CIRCUIT, RR_SPRINT, RR_DRAG, RR_DRIFT, RR_STREETX, RR_URL, RR_KNOCKOUT };

static int committed(uint32_t a, uint32_t n){
    if (!a || a + n < a) return 0;
    for (uint32_t pg = a >> 14; pg <= (a + n - 1) >> 14; pg++) if (!mem_is_committed(pg << 14)) return 0;
    return 1;
}
static uint32_t rd(uint32_t a){ return committed(a, 4) ? MR32(a) : 0; }
static uint8_t rd8(uint32_t a){ return committed(a, 1) ? MR8(a) : 0; }
static float rdf(uint32_t a){ if (!committed(a, 4)) return 0.f; float f = MRF32(a); return f == f ? f : 0.f; }

static struct {
    int raced;                     /* a race has been on since the last result */
    long screen_at;                /* the frame its results screen came (0: not up) */
    int published;                 /* this race's result is out */
    uint32_t state;                /* the coordinator's state at the last frame */
    int slot[4];
    double n;
} R = { .slot = { -2, -2, -2, -2 } };

static int results_up(void){
    static const char *const k[] = { "UI_PostRaceResults.fng", "UI_PostRace_TournResults.fng", 0 };
    const char *t = autotest_top_fng();
    for (int i = 0; k[i]; i++) if ((t && !strcasecmp(t, k[i])) || career_fe_active(k[i])) return 1;
    return 0;
}

static int race_type(void){
    if (rd8(G_RACEP + R_DRAG))  return RR_DRAG;
    if (rd8(G_RACEP + R_DRIFT)) return RR_DRIFT;
    if (rd8(G_RACEP + R_SHORT)) return RR_STREETX;
    if (rd8(G_RACEP + R_URL))   return RR_URL;
    if (rd(G_RACEP + R_KIND) == 3u) return RR_KNOCKOUT;
    return rd(G_RACEP + R_P2P) ? RR_SPRINT : RR_CIRCUIT;
}

static unsigned clampu(double v, unsigned max){ return v <= 0 ? 0u : v >= max ? max : (unsigned)(v + 0.5); }

/* the race's result into the slots (and the log); 0 when there is none to read (no rows, ours not among them) */
static int publish(void){
    uint32_t n = rd(G_RESULTS_N), pl = rd(G_PLAYER1), car = pl ? rd(pl + 4u) : 0;
    if (!n || !car || !committed(car + 0xau, 2)) return 0;
    if (n > RESULTS_MAX) n = RESULTS_MAX;
    int ours = (int)(uint16_t)MR16(car + 0xau), drift = race_type() == RR_DRIFT;
    struct { int finished, ours; double score, laps, top, dist; } row[RESULTS_MAX]; int nr = 0;
    double far = 0;
    for (uint32_t k = 0; k < n; k++){
        uint32_t x = rd(G_RESULTS + 4u * k);
        if (!committed(x, ROW_SIZE)) continue;
        uint32_t t = MR32(x + 0xcu);
        double laps = rdf(x + 0xc8u), secs = t < 4000u * 3600u ? t / 4000.0 : 0;
        row[nr].ours = (int)(MR32(x + 4u) & 0xffffu) == ours;
        row[nr].laps = laps >= 0 && laps < 1000 ? laps : 0;
        row[nr].score = drift ? rdf(x + 0x104u) : secs * 1000.0;
        row[nr].top = rdf(x + 0x10cu);
        row[nr].dist = rdf(x + 0x110u) * secs;
        row[nr].finished = MR32(x + 8u) == 1u;
        {   /* a lobby race's AI racer (its id its racer index, 100 ..: lobbyai.c): no network record behind its row, so
             * +8 is not the game's; finished by its laps (below) and a time */
            uint32_t id = MR32(x + 4u) & 0xffffu;
            if (id >= 100u && id < 116u) row[nr].finished = t > 0u && t < 4000u * 3600u;
        }
        if (row[nr].laps > far) far = row[nr].laps;
        nr++;
    }
    int me = -1, place = 0, p = 1;
    for (int k = 0; k < nr; k++){
        /* finished: as mprace.c has it, as many whole laps as the farthest (a racer still on the way when the race
         * ended has a row too) */
        row[k].finished = row[k].finished && far >= 1 && floor(row[k].laps) >= floor(far);
        if (row[k].ours){ me = k; place = row[k].finished ? p : 0; }
        p += row[k].finished;
    }
    if (me < 0) return 0;
    /* the margin: a win over the next finisher, else behind the winner (the rows are in finishing order) */
    double margin = 0;
    int next = -1, first = -1;
    for (int k = 0; k < nr; k++) if (row[k].finished){ if (first < 0) first = k; else if (next < 0 && first != k) next = k; }
    if (place == 1 && next >= 0) margin = fabs(row[next].score - row[me].score);
    else if (place > 1 && first >= 0) margin = fabs(row[me].score - row[first].score);
    if (!drift) margin /= 10.0;                                                    /* ms -> centiseconds */

    uint32_t gm = rd(G_GAMEMODE), base = rd(G_RACEP + R_RACERS), hi = rd(G_RACEP + R_HUMAN), nracers = rd(G_RACEP + R_NRACERS);
    unsigned mode = (gm & 0x200u) || rd8(G_NET_ONLINE) ? 2u : gm == 1u ? 1u : 0u;
    unsigned cartype = base && hi < nracers && nracers <= 32 ? rd(base + hi * 0x940u + 0x10u) : 127u;
    if (cartype > 126u) cartype = 127u;
    unsigned track = rd(G_RACEP + R_TRACK) & 0xffffu, rev = rd(G_RACEP + R_REVERSE) == 1u, type = (unsigned)race_type();
    unsigned laps = clampu(rd(G_RACEP + R_LAPS), 31), fin = place > 0;
    double score = fin ? row[me].score : 0;
    uint64_t race = (uint64_t)track | (uint64_t)rev << 16 | (uint64_t)type << 17 | (uint64_t)laps << 20 |
                    (uint64_t)clampu(place, 7) << 25 | (uint64_t)fin << 28 | (uint64_t)mode << 29 |
                    (uint64_t)cartype << 31 | (uint64_t)clampu(nr, 7) << 38;
    uint64_t more = (uint64_t)clampu(row[me].top * 36.0, 4095) | (uint64_t)clampu(margin, 65535) << 12 |
                    (uint64_t)clampu(row[me].dist, (1u << 20) - 1u) << 28;
    if (R.slot[0] == -2){
        R.slot[0] = host_status_slot("rr_n"); R.slot[1] = host_status_slot("rr_race");
        R.slot[2] = host_status_slot("rr_score"); R.slot[3] = host_status_slot("rr_more");
    }
    double v[4] = { ++R.n, (double)race, floor(score + 0.5), (double)more };
    if (R.slot[0] >= 0 && R.slot[1] >= 0 && R.slot[2] >= 0 && R.slot[3] >= 0) host_status_set_n(R.slot, v, 4);
    fprintf(stderr, "[race] f=%ld result %.0f: track %u%s, type %u, %u laps, %s, place %d of %d, %s %.0f, margin %.0f, "
            "top %.1f km/h, %.0f m, car type %u, mode %u\n", autotest_frame(), R.n, track, rev ? " reversed" : "", type,
            laps, fin ? "finished" : "not finished", place, nr, drift ? "points" : "ms", score, margin,
            row[me].top * 3.6, row[me].dist, cartype, mode);
    return 1;
}

/* ---- the career's state (cr_state) ---- */
static int ev_won(uint32_t h){
    uint32_t n = rd(G_CAREER + C_NEVENTS);
    if (!h || n > 0x181u || !committed(G_CAREER + C_EVENTS, 8u * n)) return 0;
    for (uint32_t i = 0; i < n; i++) if (MR32(G_CAREER + C_EVENTS + 8u * i) == h) return MR8(G_CAREER + C_EVENTS + 6u + 8u * i) == 1;
    return 0;
}
static int race_won_named(const char *name){
    uint32_t t = rd(G_RACES), n = rd(G_NRACES), pool = rd(G_STRPOOL);
    if (!t || !pool || n > 512u || !committed(t, n * 0x88u)) return 0;
    for (uint32_t i = 0; i < n; i++){
        uint32_t r = t + i * 0x88u, nm = pool + MR16(r);
        if (committed(nm, 32) && !strncasecmp((const char *)g_mem + nm, name, 32) && ev_won(MR32(r + 8u))) return 1;
    }
    return 0;
}
/* cr_more: the stages' races by their names (S<s>_...), won or not */
static double career_more(char *why, size_t n){
    why[0] = 0;
    if (rd(G_GAMEMODE) != 1u) return 0;
    uint32_t t = rd(G_RACES), cnt = rd(G_NRACES), pool = rd(G_STRPOOL);
    if (!t || !pool || cnt > 512u || !committed(t, cnt * 0x88u)) return 0;
    unsigned all[6] = {0}, won[6] = {0}, suv[6] = {0}, suvwon[6] = {0};
    for (uint32_t i = 0; i < cnt; i++){
        uint32_t r = t + i * 0x88u, nm = pool + MR16(r);
        if (!committed(nm, 32)) continue;
        const char *x = (const char *)g_mem + nm;
        if (x[0] != 'S' || x[1] < '2' || x[1] > '5' || x[2] != '_') continue;
        int st = x[1] - '0', w = ev_won(MR32(r + 8u));
        if (!strncmp(x + 3, "SUV_", 4)){ suv[st]++; suvwon[st] += w; continue; }
        if (!strncmp(x + 3, "SPON_", 5) || !strncmp(x + 3, "H_", 2)) continue;
        all[st]++; won[st] += w;
    }
    uint64_t v = 1; int k = 0;
    for (int st = 2; st <= 5; st++){
        if (all[st] && won[st] == all[st]) v |= 1ull << st;
        if (suv[st] && suvwon[st] == suv[st]) v |= 1ull << (4 + st);
        k += snprintf(why + k, n - (size_t)k, "%sstage %d %u of %u, SUV %u of %u", st > 2 ? "; " : "", st, won[st], all[st], suvwon[st], suv[st]);
        if (k >= (int)n) break;
    }
    return (double)v;
}
static double career_state(int *mags, int *dvds){
    *mags = *dvds = 0;
    if (rd(G_GAMEMODE) != 1u) return 0;
    uint32_t stage = rd(G_CAREER + C_STAGE);
    if (stage > 6u || !committed(G_CAREER + C_STAGES, 6u * C_STAGE_SZ)) return 0;
    uint64_t v = 1;
    for (uint32_t s = 0; s <= 5u; s++){
        uint32_t rec = G_CAREER + C_STAGES + s * C_STAGE_SZ, outruns = MR32(rec + 0xdcu), info = MR32(rec + 0x100u);
        if (outruns > 15u) outruns = 15u;
        if (s == 0){ v |= (uint64_t)outruns << 33; continue; }
        v |= (uint64_t)(MR8(rec + 0xfcu) != 0) << s | (uint64_t)outruns << (2 + 4 * s);
        /* (a stage without a final, its hash 0, is complete by its races, 0x532C60 -> 0x5130A0: stage 1; its last race
         * is its final here) */
        uint32_t fin = committed(info + 0x28u, 4) ? MR32(info + 0x28u) : 0;
        if (fin ? ev_won(fin) : MR8(rec + 0xfcu) != 0) v |= 1ull << (26 + s);
    }
    if (stage >= 1u) v |= 1ull << 32;
    else if (race_won_named("STAGE_0_CIRCUIT_0") && race_won_named("STAGE_0_CIRCUIT_1") && race_won_named("STAGE_0_SPRINT_0"))
        v |= 1ull << 37;
    uint32_t t = rd(G_SHOWS), n = rd(G_NSHOWS);
    for (uint32_t i = 0; t && n <= 128u && i < n && committed(t + i * 0x40u, 0x40u); i++){
        uint32_t q = t + i * 0x40u;
        if (!ev_won(MR32(q + 0x20u))) continue;
        if (MR8(q + 0x34u) == 4u) ++*dvds; else ++*mags;
    }
    v |= (uint64_t)(*mags > 63 ? 63 : *mags) << 38 | (uint64_t)(*dvds > 63 ? 63 : *dvds) << 44 |
         (uint64_t)(rd8(G_DIFFICULTY) & 3u) << 50;
    return (double)v;
}
static void career_frame(void){
    static int slot[2] = { -2, -2 }; static double was = -1, was_more = -1; static long next;
    long f = autotest_frame();
    if (f < next) return;
    next = f + 30;
    int mags, dvds; char why[256];
    double v = career_state(&mags, &dvds), more = career_more(why, sizeof why);
    if (more != was_more){
        was_more = more;
        if (more) fprintf(stderr, "[career] f=%ld races won: %s\n", f, why);
    }
    if (slot[0] == -2){ slot[0] = host_status_slot("cr_state"); slot[1] = host_status_slot("cr_more"); }
    double vals[2] = { v, more };
    if (slot[0] >= 0 && slot[1] >= 0) host_status_set_n(slot, vals, 2);
    if (v == was) return;
    was = v;
    if (!v){ fprintf(stderr, "[career] f=%ld state: not in the career\n", f); return; }
    uint64_t b = (uint64_t)v;
    fprintf(stderr, "[career] f=%ld state: stage %u, stages complete %02x, finals won %02x, outrun wins %u %u %u %u %u %u, "
            "prologue's races %s, covers: %d magazine(s), %d DVD(s) (of %u cover shoots), difficulty %u\n", f,
            rd(G_CAREER + C_STAGE), (unsigned)(b >> 1 & 0x1f), (unsigned)(b >> 27 & 0x1f), (unsigned)(b >> 33 & 15),
            (unsigned)(b >> 6 & 15), (unsigned)(b >> 10 & 15), (unsigned)(b >> 14 & 15), (unsigned)(b >> 18 & 15),
            (unsigned)(b >> 22 & 15), b >> 37 & 1 ? "won" : "-", mags, dvds, rd(G_NSHOWS), (unsigned)(b >> 50 & 3));
}

/* A race is raced from the frame its coordinator goes into "racing" (4, from its countdown or a restart): it stays
 * there behind the results screen too, and the results screen is not the top package every frame (another one
 * comes over it now and then), so neither "racing" nor the screen alone tells one race from the next.  One result a
 * race. */
void raceresult_frame(CPU *c){
    (void)c;
    career_frame();
    uint32_t rs = rd(G_RSTATUS), st = rs ? rd(rs) : 0;
    if (st == 4u && R.state != 4u && rd(G_RACEKIND) != 7u){ R.raced = 1; R.published = 0; }   /* (an outrun: none) */
    R.state = st;
    if (!results_up()){
        /* the screen went before its result was read (Continue at once): the rows are still there, read them now */
        if (R.screen_at && R.raced && !R.published && publish()){ R.published = 1; R.raced = 0; }
        R.screen_at = 0;
        return;
    }
    if (R.published || !R.raced) return;
    long f = autotest_frame();
    if (!R.screen_at) R.screen_at = f ? f : 1;
    if (f - R.screen_at < RR_SETTLE) return;
    if (publish()){ R.published = 1; R.raced = 0; }
}

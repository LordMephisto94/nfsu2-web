/* raceagain.c -- Race again: races of the stages already finished come back on
 * the map, a few at a time, and winning one pays its prize again.
 * race_again = on (NFSU2_RACE_AGAIN=1); off by default, the original game.
 *
 * The career's open races (Career +0x7148 + 0x80: 64 race records, the map's
 * and the radar's markers, the races a marker's Engage enters, 0x527780) are
 * made again by 0x527D30 after every win and on a stage's start: a random
 * pick of the current stage's races not won yet (0x527BB0, up to the stage's
 * caps), its sponsor and URL races, its hidden ones.  Each goes in by
 * 0x5007A0 (thiscall the list (race), ret 4: the first empty slot, and the
 * kind's count at +0x180 one up).  A race won once never comes back.
 *
 * Here, after the game's own list, up to RA_COUNT races of the stages before
 * the current one join it, each a normal race (kind 3, not a story race, not
 * its stage's final, not an SUV race, not hidden) the career has won, its
 * stage complete; a new random few each time the list is made.  Their markers
 * and Engage are the game's.  A win of one is the game's RaceWon (0x541420):
 * the event stays won, the prize goes into the bank as on the first win (it
 * pays whenever a race is being run, won or not before), the margin into that
 * stage's record; the stage is complete already (0x532C60 does nothing). The
 * list is made again at once, so the race leaves the map with the others.
 * Off again, the next list is the game's own: a profile keeps nothing else. */
#include "cpu.h"
#include "guest.h"
#include "game.h"
#include "setting.h"
#include <stdio.h>

#define C_OPEN          0x7148u       /* Career: the open races' list (its races at +0x80, 64 of them) */
#define OL_RACES        0x80u
#define OL_MAX          64u
#define C_NEVENTS       0x7afcu       /* Career: events with a record, at +0x7B00, 8 bytes {hash, ..., +6 state (1 won)} */
#define C_EVENTS        0x7b00u
#define C_MAXEVENTS     0x181u
#define FN_OL_ADD       0x005007a0u   /* thiscall open list (race), ret 4 */
#define RACE_SZ         0x88u
#define R_HASH          0x08u         /* race: its event hash */
#define R_SUV           0x0du         /* race: 1 an SUV race */
#define R_TYPE          0x0fu         /* race: 3 a story race */
#define R_MARKER        0x28u         /* race: its marker's trigger, 0 none */
#define R_KIND          0x34u         /* race: 1 sponsor, 2 URL, 3 normal */
#define R_STAGE         0x37u
#define R_HIDDEN        0x7fu
#define SR_COMPLETE     0xfcu         /* stage record: the stage complete */
#define SR_INFO         0x100u        /* stage record: its stage info (+0x28 the final event's hash) */
#define RA_COUNT        6u
#define RA_CANDS        256u

static int g_on = -1;
static uint32_t g_rng;

static int won(uint32_t h){
    uint32_t n = MR32(G_CAREER + C_NEVENTS);
    if (!h || n > C_MAXEVENTS) return 0;
    for (uint32_t i = 0; i < n; i++){
        uint32_t e = G_CAREER + C_EVENTS + 8u * i;
        if (MR32(e) == h) return MR8(e + 6u) == 1u;
    }
    return 0;
}
static int listed(uint32_t r){
    for (uint32_t i = 0; i < OL_MAX; i++) if (MR32(G_CAREER + C_OPEN + OL_RACES + 4u * i) == r) return 1;
    return 0;
}
static uint32_t empty_slots(void){
    uint32_t n = 0;
    for (uint32_t i = 0; i < OL_MAX; i++) n += !MR32(G_CAREER + C_OPEN + OL_RACES + 4u * i);
    return n;
}
static uint32_t rnd(uint32_t n){
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5;
    return n ? g_rng % n : 0;
}

void sub_00527d30_orig(CPU *c);
void sub_00527d30(CPU *c){
    uint32_t self = c->ecx;
    sub_00527d30_orig(c);
    if (self != G_CAREER) return;
    if (g_on < 0) g_on = setting_bool("NFSU2_RACE_AGAIN", 0);
    if (!g_on) return;
    uint32_t stage = MR32(G_CAREER + C_STAGE), tab = MR32(G_RACES), nr = MR32(G_NRACES);
    if (stage < 2u || stage > 6u || !tab || nr > 1024u || !guest_ok_bytes(tab, nr * RACE_SZ)) return;
    uint32_t cand[RA_CANDS], nc = 0;
    for (uint32_t i = 0; i < nr && nc < RA_CANDS; i++){
        uint32_t r = tab + i * RACE_SZ, s = MR8(r + R_STAGE), h = MR32(r + R_HASH);
        if (s < 1u || s >= stage || MR8(r + R_KIND) != 3u || MR8(r + R_TYPE) == 3u || MR8(r + R_SUV) || MR8(r + R_HIDDEN)) continue;
        if (!MR32(r + R_MARKER) || !won(h) || listed(r)) continue;
        uint32_t sr = G_CAREER + C_STAGES + s * C_STAGE_SZ, si = MR32(sr + SR_INFO);
        if (!MR8(sr + SR_COMPLETE)) continue;
        if (guest_ok_bytes(si, 0x2cu) && MR32(si + 0x28u) == h) continue;             /* the stage's final */
        cand[nc++] = r;
    }
    if (!g_rng) g_rng = 0x9e3779b9u ^ MR32(G_CAREER + C_NEVENTS) * 2654435761u;
    uint32_t want = RA_COUNT, room = empty_slots(), added = 0;
    if (want > room) want = room;
    while (added < want && nc){
        uint32_t k = rnd(nc), r = cand[k];
        cand[k] = cand[--nc];
        gcall(c, FN_OL_ADD, G_CAREER + C_OPEN, 1, &r);
        added++;
    }
    if (added) fprintf(stderr, "[career] race again: %u of the finished stages' races back on the map (stage %u)\n", added, stage);
}

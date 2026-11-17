/* uniques.c -- once Caleb is beaten, the unique visual parts (the wide body
 * kits and the other visual rewards of the stages' outruns) are in every
 * career car's shops.
 *
 * The game gives each unique visual part only to the one car driven to its
 * star (STAGE_2_VISUAL_1 .. STAGE_5_VISUAL_5, the last one the wide body kit),
 * and the stage-5 star is gone once the last URL race is won.  Its own ending
 * opens all of them after Caleb, but only in Customize mode, whose cars free
 * roam never drives: there the career car is the one everybody sees.  So
 * after Caleb the career shops sell them for every car too, the rule the game
 * already has for Customize.  The stars and the outrun rewards stay as they
 * are.  The unique performance parts (a separate system: UI_ChooseUniquePart,
 * RideInfo +0x318) come after Caleb every 3 outrun wins to the car driven,
 * until it has all three (below).
 *
 * The game's checks (v1.2 NA):
 *   0x52A010 IsUnlocked(filter, slot, part, car): the shops' and the garage's
 *     test of a car part (slot = CARSLOTID, WIDE_BODY 6; part = 14-byte record,
 *     byte +5's top 3 bits its unlock level, 4 = unique).  The filter comes from
 *     0x50F0D0: in career ([0x850078] 1) 6 in the shops and 2 in the garage
 *     ([0x83898C] 1), in Customize ([0x850078] 8) 0x08..0x1F8.
 *   0x515500(filter, slot, part): the career's half.  Bit 2: the car owns the
 *     part (the car's record +0x434, set by a buy, 0x5289F0).  Bit 4: the stage
 *     has opened its level (0x515360), and level 4 never.  Also called with 4
 *     alone from 0x530004 (a shop's category).
 *   0x529D60: Customize's half; unlocks level 4 once the final event of stage 5
 *     is won: Career 0x500B80([[0x86262C] + 0x28]), [0x86262C] being stage 5's
 *     stage info (its record +0x100) and +0x28 its final event, S5_CIRCUIT_36.
 * Here 0x515500 also says yes to a level-4 part with bit 4 asked once that same
 * event is won.  Nothing else changes: such a part is bought like any other and
 * is then the car's, in the profile too. */
#include "cpu.h"
#include "guest.h"
#include "game.h"
#include "splitscreen_mem.h"
#include <stdio.h>
#include <string.h>

#define C_NEVENTS      0x7afcu        /* career: events with a record */
#define C_EVENTS       0x7b00u        /* career: those records, 8 bytes {hash, ..., +6 state (1 won)} */
#define C_MAXEVENTS    0x181u         /* room up to +0x8708 */
#define G_S5_INFO      0x0086262cu    /* stage 5's stage info (its stage record +0x100) */
#define SI_FINAL       0x28u          /* stage info: the final event's hash */
#define P_LEVEL        5u             /* part: top 3 bits its unlock level */
#define LEVEL_UNIQUE   4u
#define F_STAGE        4u             /* 0x515500's filter: the stage's unlocks (the shops) */

/* Career 0x500B80 with stage 5's final event: Caleb's race won */
int uniques_caleb_beaten(void){
    uint32_t si = MR32(G_S5_INFO);
    if (!guest_ok_bytes(si, SI_FINAL + 4u)) return 0;
    uint32_t h = MR32(si + SI_FINAL), n = MR32(G_CAREER + C_NEVENTS);
    if (!h || n > C_MAXEVENTS) return 0;
    for (uint32_t i = 0; i < n; i++){
        uint32_t r = G_CAREER + C_EVENTS + 8u * i;
        if (MR32(r) == h) return MR8(r + 6u) == 1u;
    }
    return 0;
}

static int g_said;

void sub_00515500_orig(CPU *c);
void sub_00515500(CPU *c){
    uint32_t filter = MR32(c->esp + 4u), slot = MR32(c->esp + 8u), part = MR32(c->esp + 12u);
    sub_00515500_orig(c);
    if ((c->eax & 0xffu) || !(filter & F_STAGE) || !guest_ok_bytes(part, P_LEVEL + 1u)) return;
    if ((MR8(part + P_LEVEL) >> 5) != LEVEL_UNIQUE || !uniques_caleb_beaten()) return;
    c->eax = (c->eax & ~0xffu) | 1u;
    if (!g_said){
        g_said = 1;
        fprintf(stderr, "[career] Caleb beaten: the unique visual parts are in every car's shops (first asked: slot %u)\n", slot);
    }
}

/* ---- the unique performance parts, after Caleb ---------------------------------------------------
 * The game gives three, each once (career GLOBALB's "other events", [0x838424] x [0x838420], 0x18 bytes: +0 name,
 * +4 stage, +6 2 = opened by the stage's outrun wins, stage record +0xDC, reaching +7): STAGE_3_PERF_1 at 3,
 * STAGE_4_PERF_2 at 9, STAGE_5_PERF_3 at 6.  An outrun's end (0x5413E5) counts a win on the current stage's record
 * (after Caleb stage 6's), then 0x533A90(won) texts the player for each event of the stage whose count is exactly
 * its +7 (0x5338C0 on the inbox 0x860A60: the unique parts' message), and 0x533310 makes the lists again: 0x527D30
 * puts an event not won yet (Career +0x7B00 entry +6 != 1, 0x527970) in Career +0x7148's 32 slots, its marker the
 * way to THE_UNIQUES_SHOP, where UI_ChooseUniquePart gives the car driven one part (RideInfo +0x318: 10 words, one
 * a category, nonzero = its unique part).  After Caleb 0x527D30 opens nothing (stage 5's final won: 0x527E3B).
 *
 * Here, after Caleb: every 3rd outrun win (stage 6's count a multiple of 3) while the car driven has fewer than
 * three unique parts, the event of its next part (its first STAGE_3_PERF_1, then STAGE_4_PERF_2, then
 * STAGE_5_PERF_3: each its own time trial at its own marker) is open again (its entry back to not won) with the
 * game's message, and the lists keep it (uniques_perf_open, from raceagain.c's 0x527D30) until the shop gives the
 * part. */
#define G_OTHERS       0x00838424u
#define G_NOTHERS      0x00838420u
#define G_STRPOOL_U    0x00838428u
#define OTHER_SZ       0x18u
#define C_OTHERS_OPEN  0x7148u        /* Career: the other events open, 32 slots */
#define G_INBOX        0x00860a60u
#define FN_ENTRY       0x00512f10u    /* thiscall Career (hash): the event entry, made if none */
#define FN_PERF_TEXT   0x005338c0u    /* thiscall inbox (other event), ret 4: its message */
#define FN_SMS_ENTRY   0x00501310u    /* cdecl (event hash, other event +8) -> its SMS table entry, 0 none */
#define IN_COUNT       0x1400u        /* inbox: how many messages, 20 bytes each from +0 (+0 the entry) */
#define IN_SZ          0x14u
#define IN_MAX         256u
#define FN_HASH        0x0043db50u    /* cdecl (string) -> bStringHash */
/* the car's next part comes by the game's event of that number: each its own time trial at its own marker */
static const char *const PERF_EVENTS[3] = { "STAGE_3_PERF_1", "STAGE_4_PERF_2", "STAGE_5_PERF_3" };
#define PERF_EVERY     3u
#define PERF_MAX       3

static uint32_t perf_event(int k){
    uint32_t t = MR32(G_OTHERS), n = MR32(G_NOTHERS), pool = MR32(G_STRPOOL_U);
    if (k < 0 || k >= PERF_MAX || !t || !pool || n > 256u || !guest_ok_bytes(t, n * OTHER_SZ)) return 0;
    for (uint32_t i = 0; i < n; i++){
        uint32_t e = t + i * OTHER_SZ, s = pool + MR16(e);
        char nm[24];
        if (!guest_ok_bytes(s, 1u)) continue;
        guest_read_str(s, nm, (int)sizeof nm);
        if (!strcmp(nm, PERF_EVENTS[k])) return e;
    }
    return 0;
}
static int is_perf_event(uint32_t e){
    for (int k = 0; k < PERF_MAX; k++) if (e && e == perf_event(k)) return 1;
    return 0;
}
/* the car driven's unique performance parts, -1 none known */
static int perf_count(void){
    uint32_t p = MR32(SPLIT_PLAYERS_INDEX), car = p ? MR32(p + 4u) : 0, ride = car ? MR32(car + 0x10u) : 0;
    if (!guest_ok_bytes(ride, 0x340u)) return -1;
    int n = 0;
    for (uint32_t i = 0; i < 10u; i++) n += MR32(ride + 0x318u + 4u * i) != 0;
    return n;
}
/* the event's entry (Career +0x7B00), 0 none */
static uint32_t perf_entry(uint32_t h){
    uint32_t n = MR32(G_CAREER + C_NEVENTS);
    if (!h || n > C_MAXEVENTS) return 0;
    for (uint32_t i = 0; i < n; i++) if (MR32(G_CAREER + C_EVENTS + 8u * i) == h) return G_CAREER + C_EVENTS + 8u * i;
    return 0;
}
static uint32_t perf_hash(CPU *c, uint32_t e){
    uint32_t s = MR32(G_STRPOOL_U) + MR16(e);
    return gcall(c, FN_HASH, 0, 1, &s);
}

void sub_00533a90_orig(CPU *c);
void sub_00533a90(CPU *c){
    int won = (MR32(c->esp + 4u) & 0xffu) != 0;
    sub_00533a90_orig(c);
    if (!won || MR32(G_CAREER + C_STAGE) != 6u || !uniques_caleb_beaten()) return;
    uint32_t wins = MR32(G_CAREER + C_STAGES + 6u * C_STAGE_SZ + 0xdcu);
    int have = perf_count();
    uint32_t e = perf_event(have);
    if (!e || !wins || wins % PERF_EVERY || have < 0 || have >= PERF_MAX) return;
    uint32_t h = perf_hash(c, e), en = gcall(c, FN_ENTRY, G_CAREER, 1, &h);
    if (!guest_ok_bytes(en, 8u)) return;
    uint8_t was = MR8(en + 6u);
    MR8(en + 6u) = 0;
    /* the game adds a message only while its entry is not in the inbox (0x514CB0): the last part's text goes */
    uint32_t eh = MR32(e + 8u), sms = gcall(c, FN_SMS_ENTRY, 0, 1, &eh), n = MR32(G_INBOX + IN_COUNT), gone = 0;
    if (sms && n <= IN_MAX)
        for (uint32_t i = 0; i < n; ){
            uint32_t m = G_INBOX + i * IN_SZ;
            if (MR32(m) != sms){ i++; continue; }
            for (uint32_t k = i; k + 1u < n; k++)
                for (uint32_t b = 0; b < IN_SZ; b += 4u) MR32(G_INBOX + k * IN_SZ + b) = MR32(G_INBOX + (k + 1u) * IN_SZ + b);
            n--; gone++;
            MR32(G_INBOX + IN_COUNT) = n;
        }
    gcall(c, FN_PERF_TEXT, G_INBOX, 1, &e);
    if (gone) fprintf(stderr, "[career] after Caleb: the last unique part text taken out of the inbox (%u)\n", gone);
    fprintf(stderr, "[career] after Caleb: %s's entry was %u, now 0\n", PERF_EVENTS[have], was);
    fprintf(stderr, "[career] after Caleb: outrun win %u, the car has %d of %d unique performance parts: one more at the shop\n",
            wins, have, PERF_MAX);
}
/* raceagain.c's 0x527D30 (the lists made again): the event stays open while it waits for its part */
void uniques_perf_open(CPU *c){
    if (MR32(G_CAREER + C_STAGE) != 6u || !uniques_caleb_beaten()) return;
    int have = perf_count();
    if (have < 0 || have >= PERF_MAX) return;
    uint32_t e = perf_event(have);
    if (!e) return;
    uint32_t en = perf_entry(perf_hash(c, e));
    static int last = -2;
    int st = en ? MR8(en + 6u) : -1, how = 0;
    if (en && st != 1){
        uint32_t i;
        for (i = 0; i < 32u; i++) if (MR32(G_CAREER + C_OTHERS_OPEN + 4u * i) == e) break;
        if (i < 32u) how = 1;
        else for (i = 0; i < 32u; i++)
            if (!MR32(G_CAREER + C_OTHERS_OPEN + 4u * i)){ MR32(G_CAREER + C_OTHERS_OPEN + 4u * i) = e; how = 2; break; }
    }
    if (st != last || how != 1){
        last = st;
        fprintf(stderr, "[career] after Caleb: %s entry %s (state %d), car has %d: %s\n", PERF_EVENTS[have], en ? "found" : "none", st, have,
                how == 2 ? "opened" : how == 1 ? "open already" : "not opened");
    }
}

/* The unique part screen's offer, 0x533D20 (thiscall Career +0xC00, the story: its +8 the other event being run):
 * the event's +0x15.. categories (up to 3, ending at 0 or -1; a perf event's: STAGE_3_PERF_1 1 5 7, STAGE_4_PERF_2
 * 2 9 8, STAGE_5_PERF_3 4 6 3, the 9 between them) each into its list (this +0xC, how many +0x110; returned) by
 * 0x502240(category): a perf one only when the car driven (its record, 0x503680 on the car table 0x83AD90 with
 * Career +0x8740) has none there yet (record +0x2F4 + 4 x that).  After Caleb the three perf events offer all 9 instead,
 * so each car picks its three from every category. */
#define G_CARTAB       0x0083ad90u
#define C_CAR          0x8740u
#define FN_CAR_REC     0x00503680u    /* thiscall car table (handle) -> record */
#define FN_PERF_CAT    0x00502240u    /* cdecl (event's category) -> the car record's slot */
#define REC_UNIQUES    0x2f4u
#define ST_EVENT       0x08u
#define ST_LIST        0x0cu
#define ST_COUNT       0x110u
#define ST_MAXLIST     0x43u
void sub_00533d20_orig(CPU *c);
void sub_00533d20(CPU *c){
    uint32_t self = c->ecx;
    sub_00533d20_orig(c);
    uint32_t ret = c->eax;
    if (MR32(G_CAREER + C_STAGE) != 6u || !uniques_caleb_beaten() || !guest_ok_bytes(self, ST_COUNT + 4u)) return;
    uint32_t e = MR32(self + ST_EVENT);
    if (!is_perf_event(e)) return;
    uint32_t h = MR32(G_CAREER + C_CAR), rec = gcall(c, FN_CAR_REC, G_CARTAB, 1, &h);
    if (!guest_ok_bytes(rec, REC_UNIQUES + 0x40u)) return;
    uint32_t n = 0;
    for (uint32_t v = 1; v <= 9u && n < ST_MAXLIST; v++){
        uint32_t cat = gcall(c, FN_PERF_CAT, 0, 1, &v);
        if (cat >= 16u || MR32(rec + REC_UNIQUES + 4u * cat)) continue;
        MR32(self + ST_LIST + 4u * n) = cat; n++;
    }
    MR32(self + ST_COUNT) = n;
    c->eax = ret;
    fprintf(stderr, "[career] after Caleb: the unique part shop offers %u categories (all the car has none in)\n", n);
}

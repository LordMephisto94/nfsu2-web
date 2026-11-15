/* lobbyai.c -- AI racers in a free-roam lobby race: the host's choice on the lobby card (AI racers, L; roam_game.c
 * MP_AI in its adverts, MpRace.ai), the race's empty places filled with the game's own AI racers.  The host's game
 * drives them; every other game of the race has the same racers, without a driver, drawn where the host's states put
 * them (racecars.c), and takes their finishes from the host.
 *
 * The game (SPEED2.EXE) makes a network race (LAN, Online) without AI racers or traffic:
 *  - RaceStarter::StartNetworkRace (0x53FD30) sets [0x89E7E4] (a network race; Quick Race's 0x53FC20 clears it), puts
 *    1 human and 0 opponents in Quick Race's settings and picks the mode's starter (RaceStarter::Setup<mode>, 0x53F150
 *    and the next ones) by the network race's type ([0x850088]).  The starter adds the humans (0x525ED0), then hands
 *    [0x89E7C4] to its AI adder (0x53EE90) and lays out the grid (0x5264A0).  Every AI and traffic adder (0x53EE90,
 *    0x53F000, 0x4FE9F0) returns at once while [0x89E7E4] is set; other code reads it too.  So each game of the race
 *    clears it only around its own call of the AI adder, once garage.c's 0x525ED0 has the humans in
 *    (lobbyai_humans_added); the starter's own call adds none.
 *  - The race's racers ([0x89E810], [0x89E814] of them, 0x940 bytes each: +0 a flag (> 0: no car), +1 its index (0 ..
 *    the players by their network slot, 100 .. the AI racers), +3 its grid spot, +4 its kind (1 a player, 2 an AI
 *    racer), +0x10 its RideInfo, +0x90C its name).  An AI racer's car has its index as its race slot (car +0xA).
 *
 * What each part here mends, as tests of 2-player lobby races found it (2026-11-15):
 *  - the same racers on every game: the AI racers' drivers, cars and paint are the game's random picks (0x43C1C0 and
 *    its kin, one state at 0x7EA624): two games named them Marcel and Steve, and Jared and Neil.  Each game seeds that
 *    state the same for the AI adder's call, from what the lobby's players share (its LAN game's password, the career
 *    race, the track), and gets its own back after;
 *  - how many: as many as the race's cars take less the lobby's players, a drag race 4 cars, the others 6 (3 AI
 *    racers in a 2-player drag left a player's car unmade);
 *  - the grid: CreateCars (0x6065B0) goes through the grid spots 1 .. 6 and makes a car for the first racer on each.
 *    The players have the spots the host's network race gave them, the AI racers the ones the race's grid gave them
 *    before the other players came: an AI racer and a player shared a spot and the player's car was never made (the
 *    other player's car stood still on each screen).  Before CreateCars the AI racers take the spots no player has, by
 *    their indexes, the lowest first (lobbyai_grid);
 *  - the paint: each car is painted into the texture of its ride's skin number (RideInfo +0x584, the hash of
 *    "DUMMY_SKIN<n>", 0x61C280 sets it), one texture a number.  A player's number is its network slot + 1, an AI
 *    racer's its index less 98 (2, 3 ..): the host's red car was drawn in an AI racer's blue with its yellow hood
 *    vinyl.  The race's cars are painted as they come, right after the AI racers are added: there each AI racer whose
 *    number a player's slot (the network's players [0x89CF50 + 4 slot], our own one at that moment) or an AI racer
 *    before it has takes the lowest one free (1 .. 6, as a Quick Race's six racers), again before CreateCars;
 *  - the names: the results and standings screens name a racer in a network race (0x5F00F0) by its network player
 *    (0x5F0220 on 0x89CF48, its slot; +0x12 its LAN name), else by its record (+0x90C).  An AI racer's slot is past
 *    that table of 6: two screens left the name out, two took whatever was there.  Right after each of those calls an
 *    AI racer's gives its own record, less 0x12 (lobbyai_hook);
 *  - the finishes: the coordinator's FinishRacer (0x606CB0, thiscall on [0x890118]: the sorted index, a place (0: the
 *    lowest no racer holds, 0x5EEF70), a flag, how) gives a racer its place (+0xC of its entry) and, in a network
 *    race, asks the network whether it stands (0x602000 on 0x89CF48: the car's slot ...).  For an AI racer's slot
 *    that read past the table and took its finish back: the players' finish places left the AI racers out.  Here
 *    an AI racer's stands.  Only the host's game finishes them: each game timed its own copy, and a joiner's are drawn
 *    a little in the past (one 44.46 s on the host, 44.70 s on the joiner); the host sends each finish ("N2AF") and a
 *    joiner finishes its copy with the host's place and time (lobbyai_frame);
 *  - the final places: the race over, the network ranks its players (0x5FE670, once, from 0x60955F) 1, 2 .. among
 *    themselves, into each network player (+0x24) and its racer's entry (+0xC): players 3rd and 4th behind two AI
 *    racers became 1st and 2nd, and the last one's HUD said it came second.  The places FinishRacer gave come back
 *    after it, and each network player's rank is its racer's place;
 *  - (mprace.c ai_rows_sort, raceresult.c) the results' rows in the order the racers finished, an AI racer's row
 *    finished by its laps and time (it has no network record behind it).
 *
 * N2AF (the horn's port, racehorn.c; the host to each racer, three times): 0 "N2AF", 4 version 1, 5 the AI racer's
 * slot, 6 its place, 7 how it finished, 8 its time (u32 LE, the race clock's 1/4000 s, its entry's +0x198).
 *
 * Test knob (NFSU2_INPUT_SCRIPT set): NFSU2_LOBBY_AI=<n>, fewer AI racers than the race takes. */
#include "cpu.h"
#include "guest.h"
#include "game.h"
#include "mprace.h"
#include "widescreen.h"           /* HOOK_SITE */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RACERS          0x0089e810u   /* the race's racer records (above) */
#define RACERS_N        0x0089e814u
#define REC_SIZE        0x940u
#define REC_NAME        0x90cu
#define KIND_PLAYER     1u
#define KIND_AI         2u
#define AI_SLOT0        100
#define AI_SLOTS        16
#define G_RACE_HUMANS   0x0089e7c0u
#define G_RACE_AI       0x0089e7c4u   /* the AI racers the starter's adder takes */
#define G_RACE_LAPS     0x0089e7bcu
#define G_NET_RACE      0x0089e7e4u   /* a byte: a network race (RaceStarter::StartNetworkRace) */
#define G_NET_TYPE      0x00850088u   /* the network race's type: 2 a drag race */
#define NET_TYPE_DRAG   2u
#define G_NET           0x0089cf48u   /* the network race (thiscall this of 0x5F0220, 0x602000, 0x5FE670) */
#define G_RAND          0x007ea624u   /* the game's random numbers' state (0x43C1C0: xorshift) */
#define FN_ADD_RACE_AI  0x0053ee90u   /* cdecl (count): AI racers 100 .., their cars by [0x89E7F4] */
#define FN_RIDE_SKINS   0x0061c280u   /* thiscall ride (n): DUMMY_SKIN<n>, its wheels' and spinners' */
#define RIDE_SKIN       0x584u        /* a RideInfo: its skin's hash (0: its car type has none) */
#define FN_FINISH       0x00606cb0u   /* FinishRacer (above) */
#define FN_SET_DRIVER   0x005eba30u   /* Car::SetDriverType (type): 1 the player, 3 an AI racer, 0 none */
#define CAR_SLOT        0x0au
#define CAR_REC         0x14u
#define CAR_DRIVER      0x24u
#define RS_COUNT        0x24u         /* the race's status [0x890118]: its racers, */
#define RS_ENTRIES      0x30u         /*   their entries (0x1F0 each: +0 the car, +0xC the final place, +0x10 how it */
#define RS_ENTRY        0x1f0u        /*   finished, +0x14 the way done, +0x198 the time), */
#define RS_SORTED       0xbd0u        /*   the entries by place */
#define E_PLACE         0x0cu
#define E_HOW           0x10u
#define E_DONE          0x14u
#define E_TIME          0x198u
#define NP_RANK         0x24u         /* a network player: its rank */
#define AF_N            12
#define AF_PORT         3659u         /* racehorn.c's HORN_PORT */
#define AF_PEER_MS      5000u

int  vlan_rt_send(uint16_t port, uint32_t ip_h, const void *p, int n);
int  vlan_race_peers(uint32_t *ip_h, int max, uint32_t within_ms);
long autotest_frame(void);

static int ai_slot(int s){ return s >= AI_SLOT0 && s < AI_SLOT0 + AI_SLOTS; }
static int lan(void){ return (guest_rd32(G_GAMEMODE) & 0x200u) != 0; }
static int car_slot(uint32_t car){ return car && guest_ok_bytes(car + CAR_SLOT, 2u) ? (int16_t)MR16(car + CAR_SLOT) : -1; }
static uint32_t racers(uint32_t *n){
    uint32_t b = guest_rd32(RACERS); *n = guest_rd32(RACERS_N);
    return b && *n && *n <= 16u && guest_ok_bytes(b, *n * REC_SIZE) ? b : 0;
}

/* ---- the racers ---- */
static int ai_count(const MpRace *r){
    if (!r->ai) return 0;
    int cars = guest_rd32(G_NET_TYPE) == NET_TYPE_DRAG ? 4 : 6, n = cars - (r->players > 0 ? r->players : 2);
    const char *sc = getenv("NFSU2_INPUT_SCRIPT"), *e = sc && *sc ? getenv("NFSU2_LOBBY_AI") : 0;
    if (e && *e && atoi(e) >= 0 && atoi(e) < n) n = atoi(e);
    return n < 0 ? 0 : n;
}
static uint32_t skin_hash(int k){
    char b[16]; snprintf(b, sizeof b, "DUMMY_SKIN%d", k);
    uint32_t h = 0xffffffffu;
    for (const char *q = b; *q; q++) h = h * 33u + (uint8_t)*q;           /* bStringHash */
    return h;
}
static int skin_number(uint32_t ride){
    uint32_t h = guest_rd32(ride + RIDE_SKIN);
    for (int k = 1; h && k <= 8; k++) if (h == skin_hash(k)) return k;
    return 0;
}
/* the AI racers' skin numbers off the players' (their slots + 1, their rides') and one another's */
static void ai_skins(CPU *c, const char *when){
    uint32_t n, base = racers(&n);
    if (!base) return;
    unsigned used = 0;
    for (uint32_t k = 0; k < 6u; k++) if (guest_rd32(G_NETPLAYERS + 4u * k)) used |= 1u << (k + 1u);
    for (uint32_t i = 0; i < n; i++){
        uint32_t r = base + i * REC_SIZE;
        if ((int8_t)MR8(r) <= 0 && guest_rd32(r + 4u) == KIND_PLAYER){ int k = skin_number(r + 0x10u); if (k) used |= 1u << k; }
    }
    for (uint32_t i = 0; i < n; i++){
        uint32_t r = base + i * REC_SIZE;
        if ((int8_t)MR8(r) > 0 || guest_rd32(r + 4u) != KIND_AI) continue;
        int k = skin_number(r + 0x10u), to = k;
        if (!k) continue;                                                    /* (a car type without a skin) */
        if (used & 1u << k){ to = 0; for (int q = 1; q <= 6 && !to; q++) if (!(used & 1u << q)) to = q; }
        if (!to){ fprintf(stderr, "[lobbyai] AI racer %u: no skin number free (%d, a player's too) %s\n", MR8(r + 1u), k, when); continue; }
        if (to != k){
            uint32_t a = (uint32_t)to; gcall(c, FN_RIDE_SKINS, r + 0x10u, 1, &a);
            fprintf(stderr, "[lobbyai] AI racer %u: skin number %d -> %d (a player's, or another's) %s\n", MR8(r + 1u), k, to, when);
        }
        used |= 1u << to;
    }
}
/* garage.c's 0x525ED0, the race's humans in: the lobby's AI racers, on every game of the race */
void lobbyai_humans_added(CPU *c){
    const MpRace *r = mprace_lobby();
    if (!lan() || !r) return;
    int want = ai_count(r);
    if (!want) return;
    uint8_t net = MR8(G_NET_RACE);
    uint32_t n = (uint32_t)want, seed = 2166136261u;
    for (int i = 0; i < (int)sizeof r->password && r->password[i]; i++) seed = (seed ^ (uint8_t)r->password[i]) * 16777619u;
    seed = (seed ^ r->race) * 16777619u; seed = (seed ^ (uint32_t)r->track) * 16777619u;
    if (!seed) seed = 1;
    uint32_t rnd = MR32(G_RAND);
    MR32(G_RACE_AI) = n; MR32(G_RAND) = seed; MR8(G_NET_RACE) = 0;
    gcall(c, FN_ADD_RACE_AI, c->ecx, 1, &n);
    MR8(G_NET_RACE) = net; MR32(G_RAND) = rnd;
    ai_skins(c, "as they came");
    char names[96]; int ln = 0; names[0] = 0;
    uint32_t nr, base = racers(&nr);
    for (uint32_t i = 0; base && i < nr; i++){
        uint32_t q = base + i * REC_SIZE;
        if (guest_rd32(q + 4u) != KIND_AI || ln >= (int)sizeof names - 20) continue;
        ln += snprintf(names + ln, sizeof names - (size_t)ln, "%s%.15s", ln ? ", " : "", (const char *)g_mem + q + REC_NAME);
    }
    fprintf(stderr, "[lobbyai] %d AI racer(s) in the race's empty places, %s: %s\n", want,
            r->host ? "the host's game drives them" : "the host drives them", names);
}
/* roam.c's 0x6065B0, before CreateCars: the AI racers' skins again, and on the grid spots no player has */
void lobbyai_grid(CPU *c){
    if (!lan() || !mprace_lobby()) return;
    ai_skins(c, "before the cars were made");
    uint32_t n, base = racers(&n);
    if (!base) return;
    unsigned used = 0; uint32_t order[16]; int k = 0;
    for (uint32_t i = 0; i < n; i++){
        uint32_t r = base + i * REC_SIZE;
        int spot = (int8_t)MR8(r + 3u);
        if ((int8_t)MR8(r) > 0) continue;
        if (guest_rd32(r + 4u) == KIND_PLAYER && spot >= 1 && spot <= 6) used |= 1u << spot;
        if (guest_rd32(r + 4u) == KIND_AI) order[k++] = r;
    }
    if (!k) return;
    for (int a = 1; a < k; a++) for (int b = a; b > 0 && MR8(order[b] + 1u) < MR8(order[b - 1] + 1u); b--){ uint32_t t = order[b]; order[b] = order[b - 1]; order[b - 1] = t; }
    char line[64]; int ln = 0; line[0] = 0;
    for (int a = 0; a < k; a++){
        int spot = 0;
        for (int g = 1; g <= 6 && !spot; g++) if (!(used & 1u << g)) spot = g;
        if (!spot) MR8(order[a]) = 1;                                        /* (no spot left: no car) */
        else { used |= 1u << spot; MR8(order[a] + 3u) = (uint8_t)spot; }
        if (ln < (int)sizeof line - 12) ln += snprintf(line + ln, sizeof line - (size_t)ln, " %u@%d", MR8(order[a] + 1u), spot);
    }
    fprintf(stderr, "[lobbyai] f=%ld the AI racers' grid spots (index@spot):%s\n", autotest_frame(), line);
}

/* ---- their names on the results ---- */
static int name_hook(CPU *c, int slot, uint32_t rec){
    if (!ai_slot(slot) || !rec || !guest_ok_bytes(rec, REC_NAME + 16u) || guest_rd32(rec + 4u) != KIND_AI) return 1;
    if (!MR8(rec + REC_NAME)) snprintf((char *)g_mem + rec + REC_NAME, 16, "AI %d", slot - AI_SLOT0 + 1);
    c->eax = rec + REC_NAME - 0x12u;
    return 1;
}
int lobbyai_hook(CPU *c, uint32_t ip){
    switch (ip){
    case HOOK_SITE(0x004bf3dc): return name_hook(c, (int)guest_rd32(c->ebp + 4u), guest_rd32(c->esp + 0x18u));
    case HOOK_SITE(0x004c03dc): return name_hook(c, (int)guest_rd32(c->edi + 4u), guest_rd32(c->esi + 0x14u));
    case HOOK_SITE(0x004c6fe5): return name_hook(c, (int16_t)MR16(c->edi + 0xau), guest_rd32(c->edi + 0x14u));
    case HOOK_SITE(0x004c93ae): return name_hook(c, (int16_t)MR16(c->edi + 0xau), guest_rd32(c->edi + 0x14u));
    }
    return 0;
}

/* ---- their finishes ---- */
static struct { int got, done; uint8_t place, how; uint32_t t; } g_af[AI_SLOTS];   /* a joiner: the host's, by slot */
static int g_af_ours;                                                              /* our own FinishRacer for one */
static int racing(void){ return lan() && mprace_lobby_racing(); }

void sub_00602000_orig(CPU *c);
void sub_00602000(CPU *c){
    if (ai_slot((int)guest_rd32(c->esp + 4u)) && lan()){ c->eax = (c->eax & ~0xffu) | 1u; POP32(c->eip); c->esp += 0x10u; return; }
    sub_00602000_orig(c);
}
static void af_send(int slot, int place, int how, uint32_t t){
    uint8_t m[AF_N]; memset(m, 0, sizeof m);
    memcpy(m, "N2AF", 4); m[4] = 1; m[5] = (uint8_t)slot; m[6] = (uint8_t)place; m[7] = (uint8_t)how;
    m[8] = (uint8_t)t; m[9] = (uint8_t)(t >> 8); m[10] = (uint8_t)(t >> 16); m[11] = (uint8_t)(t >> 24);
    uint32_t peers[16];
    int n = vlan_race_peers(peers, 16, AF_PEER_MS);
    for (int k = 0; k < 3; k++) for (int i = 0; i < n; i++) vlan_rt_send(AF_PORT, peers[i], m, AF_N);
}
void lobbyai_finish_heard(const uint8_t *m, int n, uint32_t from){
    (void)from;
    const MpRace *r = mprace_lobby();
    if (n < AF_N || memcmp(m, "N2AF", 4) || m[4] != 1 || !r || r->host || !ai_slot(m[5])) return;
    int k = m[5] - AI_SLOT0;
    if (g_af[k].got) return;
    g_af[k].got = 1; g_af[k].place = m[6]; g_af[k].how = m[7];
    g_af[k].t = (uint32_t)m[8] | (uint32_t)m[9] << 8 | (uint32_t)m[10] << 16 | (uint32_t)m[11] << 24;
}
void sub_00606cb0_orig(CPU *c);
void sub_00606cb0(CPU *c){
    uint32_t rc = c->ecx, i = guest_rd32(c->esp + 4u);
    uint32_t e = i < 8u && guest_ok_bytes(rc + RS_SORTED + 4u * i, 4u) ? guest_rd32(rc + RS_SORTED + 4u * i) : 0;
    int slot = e && guest_ok_bytes(e, 4u) ? car_slot(guest_rd32(e)) : -1;
    const MpRace *r = mprace_lobby();
    if (ai_slot(slot) && racing() && r && !r->host && !g_af_ours){ POP32(c->eip); c->esp += 0x10u; return; }   /* (the host's) */
    sub_00606cb0_orig(c);
    if (!ai_slot(slot) || !racing() || !r || !guest_ok_bytes(e, E_TIME + 4u) || !MR16(e + E_HOW)) return;
    fprintf(stderr, "[lobbyai] f=%ld AI racer %d over the line: place %d, %.2f s%s\n", autotest_frame(), slot, (int8_t)MR8(e + E_PLACE),
            (double)guest_rd32(e + E_TIME) / 4000.0, r->host ? "" : " (the host's)");
    if (r->host) af_send(slot, (int8_t)MR8(e + E_PLACE), MR16(e + E_HOW), guest_rd32(e + E_TIME));
}
/* a joiner: the host's AI finishes not taken yet, through the coordinator's own FinishRacer, with the host's time and
 * the race's whole way done (+0x14, the results' laps: our copy is drawn a little behind) */
static void finish_frame(CPU *c){
    uint32_t rc = guest_rd32(G_RACE), n = rc ? guest_rd32(rc + RS_COUNT) : 0;
    if (!n || n > 8u) return;
    for (int k = 0; k < AI_SLOTS; k++){
        if (!g_af[k].got || g_af[k].done) continue;
        for (uint32_t i = 0; i < n; i++){
            uint32_t e = guest_rd32(rc + RS_SORTED + 4u * i);
            if (!e || !guest_ok_bytes(e, E_TIME + 4u) || car_slot(guest_rd32(e)) != AI_SLOT0 + k) continue;
            uint32_t a[4] = { i, g_af[k].place, 0, g_af[k].how ? g_af[k].how : 1u };
            g_af_ours = 1; gcall_main(c, FN_FINISH, rc, 4, a); g_af_ours = 0;
            MR32(e + E_TIME) = g_af[k].t;
            float laps = (float)guest_rd32(G_RACE_LAPS);
            if (laps >= 1.f && laps < 128.f && MRF32(e + E_DONE) < laps) MRF32(e + E_DONE) = laps;
            g_af[k].done = 1;
            break;
        }
    }
}
void sub_005fe670_orig(CPU *c);
void sub_005fe670(CPU *c){
    uint32_t rc = guest_rd32(G_RACE), n = rc ? guest_rd32(rc + RS_COUNT) : 0;
    int8_t was[8]; int ai = 0;
    for (uint32_t i = 0; n <= 8u && i < n; i++){
        uint32_t e = rc + RS_ENTRIES + RS_ENTRY * i;
        was[i] = (int8_t)MR8(e + E_PLACE); ai |= ai_slot(car_slot(guest_rd32(e)));
    }
    sub_005fe670_orig(c);
    if (!ai || !racing() || !n || n > 8u) return;
    for (uint32_t i = 0; i < n; i++){
        uint32_t e = rc + RS_ENTRIES + RS_ENTRY * i;
        int sl = car_slot(guest_rd32(e));
        if (was[i] <= 0) continue;
        MR8(e + E_PLACE) = (uint8_t)was[i];
        uint32_t p = sl >= 0 && sl < 6 ? guest_rd32(G_NET + 8u + 4u * (uint32_t)sl) : 0;
        if (p && guest_ok_bytes(p + NP_RANK, 4u)) MR32(p + NP_RANK) = (uint32_t)was[i];
    }
}

/* ---- each frame of a lobby race ---- */
void lobbyai_frame(CPU *c){
    static int on;
    const MpRace *r = mprace_lobby();
    if (!racing() || !r){ if (on){ on = 0; memset(g_af, 0, sizeof g_af); } return; }
    on = 1;
    if (r->host) return;
    finish_frame(c);
    /* a joiner's AI racers' cars without a driver (as free roam's friends' cars): racecars.c puts them where the host's
     * states say */
    uint32_t rc = guest_rd32(G_RACE), n = rc ? guest_rd32(rc + RS_COUNT) : 0;
    for (uint32_t i = 0; n <= 8u && i < n; i++){
        uint32_t car = guest_rd32(rc + RS_ENTRIES + RS_ENTRY * i), rec = car && guest_ok_bytes(car + CAR_REC, 4u) ? guest_rd32(car + CAR_REC) : 0;
        if (!ai_slot(car_slot(car)) || !rec || !guest_ok_bytes(rec, 8u) || guest_rd32(rec + 4u) != KIND_AI || !guest_rd32(car + CAR_DRIVER)) continue;
        uint32_t none = 0;
        gcall_main(c, FN_SET_DRIVER, car, 1, &none);
    }
}

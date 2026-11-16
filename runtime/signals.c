/* signals.c -- turn signals and hazard lights (an experiment): `,` the left signal, `/` the right one, `'` the hazard
 * lights, each a press on and a press off (a signal turns the other one off; the hazard lights blink both and leave
 * the signal set under them as it was).  On a controller: D-pad left, right and up held half a second (a short
 * press stays the game's: dinput_pad.c signal_pad).  Only while the player drives (lightkey.c lightkey_driving); they
 * stay on through a pause and go off with a new car.  Other players see them: friends in free roam (roam_game.c: the
 * state's open parts' byte, RC_OPEN_SIGL / RC_OPEN_SIGR) and the others in a race between players (racecars.c: the
 * same state in N2RS); a player whose game is paused (their status RC_ST_PAUSED) shows the hazard lights.  The keys
 * are the player's (the start page's Controls, Site: settings.ini key_signal_left, key_signal_right, key_hazard:
 * NFSU2_KEY_SIGNAL_LEFT ...; dinput_kbd.c dikbd_site_key), and a key one of the game's controls is on stays the
 * game's (dikbd_ours).
 *
 * The cars have no indicators.  A car's light list (its render info +0x3F0, built by 0x6204D0 from the geometry's
 * light markers, chunk 0x0013401A) holds LEFT_HEADLIGHT, RIGHT_HEADLIGHT, LEFT_BRAKELIGHT, RIGHT_BRAKELIGHT and
 * CENTRE_BRAKELIGHT (bStringHash; every car in CARS/ checked: no other light names), and their parts are lamps
 * only (*_HEADLIGHT_LEFT, *_BRAKELIGHT_A..C).  So a signal is a flare of the game's own, amber: the flare routine the
 * headlights and brake lights are drawn with (0x490920, cdecl: view, light, matrix, strength, three of the view's
 * modes, colour), given a copy of the side's headlight light 0.15 m further out and of its brake light
 * 0.06 m further out, the copy's kind (+0x2C) 0, the headlights' (whose flare the colour tints: 0x4915F6), at full
 * strength, amber.  The colour is 0x00BBGGRR there: 0xFF8C00 as RRGGBB was blue on screen (a player's test,
 * 2026-11-16).  A light (0x30 bytes, 0x6204D0): +8 its hash, +0x10 its place in the car, +0x1C what the
 * flare routine keeps of its visibility, +0x20 the way it faces (a flare seen from behind it is fainter: its flag 4 in
 * +0x2D), +0x2C its kind (0 a headlight, 1 a brake light).  It is drawn after the game's own flares of the car
 * (photo.c's override of 0x615630, the car's flares: thiscall on its render info, ret 0x18; its arguments the view,
 * the car's place, its matrix, which flares, the view's mode and a strength), with a matrix from the same pool the
 * game takes its own from (0x82DA5C .. 0x82DA60), the view's mode passed on as 0x615630 passes it (0x61596F..).
 *
 * Logs ("[signals]"): each press and what is on, each friend's or racer's change, a car's lights the first time its
 * signals are drawn (how many were found: 0 means nothing can be drawn on it).  Script: "signals left|right|hazard|
 * off|report", "signals look OUT_F OUT_R RRGGBB" (the look, for variants). */
#include "cpu.h"
#include "guest.h"
#include "game.h"
#include "kbd.h"
#include "roam_core.h"            /* RC_SIG_L, RC_SIG_R */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

long autotest_frame(void);
unsigned dikbd_site_key(const char *env, unsigned dflt);   /* dinput_kbd.c */
int dikbd_ours(unsigned dik, const char *what);
const char *dik_name(unsigned dik);                       /* dikeys.c */
int lightkey_driving(void);                               /* lightkey.c: the player drives */
void pad_signal_offer(void);                              /* dinput_pad.c: D-pad held for the signals, now */
int pad_signal_take(void);                                /* ...the holds since (SG_*) */
uint32_t gheap_alloc(int heap, uint32_t n, int zero);     /* heap.c */

#define G_PLAYER1    SPLIT_PLAYERS_INDEX   /* Player*: +4 its car */
#define DIK_COMMA    0x33u
#define DIK_SLASH    0x35u
#define DIK_QUOTE    0x28u
#define FN_FLARE     0x00490920u   /* a light's flare (cdecl: view, light, matrix, strength, mode, mode, depth, colour) */
#define G_MPOOL      0x0082da5cu   /* the frame's matrix pool: the next free one ... */
#define G_MPOOL_END  0x0082da60u   /* ... and its end (0x615630 takes one 0x40 of it per car) */
#define RI_CAR       0x0u          /* a car's render info: its car (0 for the front end's) */
#define RI_LIGHTS    0x3f0u        /*   its lights: a list (+0 next), each LIGHT_SZ */
#define CAR_LEVEL    0x424u        /* a car: 0x615630 draws its flares' visibility by the view's mode below 3 */
#define LIGHT_SZ     0x30u
#define LT_HASH      0x8u
#define LT_POS       0x10u
#define LT_KIND      0x2cu
#define HASH_HL_L    0x9db90133u   /* LEFT_HEADLIGHT */
#define HASH_HL_R    0xd09091c6u   /* RIGHT_HEADLIGHT */
#define HASH_BL_L    0x31a66786u   /* LEFT_BRAKELIGHT */
#define HASH_BL_R    0xbf700a79u   /* RIGHT_BRAKELIGHT */
#define SIG_ON_S     0.40          /* the blink: on this long ... */
#define SIG_PERIOD_S 0.75          /* ... every this long (80 a minute; the law's 60 to 120) */
#define RING         512           /* light copies, used in turn (the flare routine may keep a light's address till the
                                      frame is drawn: 4 a car, 128 cars) */
#define NCARS        64            /* other players' cars a frame (roam_game.c's 64 slots, racecars.c's 8) */
enum { SG_LEFT = 1, SG_RIGHT = 2, SG_HAZARD = 4 };   /* (dinput_pad.c's holds) */

static struct {
    int turn, hazard;                 /* our signal (RC_SIG_L, RC_SIG_R, 0) and hazard lights */
    int test;                         /* the script's: -1 none, else what is on */
    uint32_t car;                     /* our car, as last seen */
    int last;                         /* what was on, as last logged */
    uint32_t kseq; int kinit; uint8_t kdown[3];
    unsigned dik[3]; int dik_set;
    struct { uint32_t car; uint8_t sig, paused; long seen; } o[NCARS];   /* other players' cars */
    long fr;                          /* frames (signals_frame) */
    uint32_t ring; uint32_t ri;       /* light copies (guest memory), the next */
    struct { uint32_t ri; int n; } said[32]; int nsaid;
    float out_f, out_r; uint32_t colour;   /* the flares' colour, RRGGBB (flare_colour: the routine's order) */
    unsigned long drawn, pool_full;
} S = { .test = -1, .last = -1, .out_f = 0.15f, .out_r = 0.06f, .colour = 0xff8c00u };

static double now_s(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec + ts.tv_nsec * 1e-9; }
static uint32_t my_car(void){ uint32_t pl = guest_rd32(G_PLAYER1); return pl ? guest_rd32(pl + 4u) : 0u; }
static const char *sig_name(int s){
    return (s & 3) == 3 ? "hazard lights" : s & RC_SIG_L ? "left signal" : s & RC_SIG_R ? "right signal" : "off";
}

/* ours now: RC_SIG_L, RC_SIG_R, both (the hazard lights) or 0 */
int signals_mine(void){
    if (S.test >= 0) return S.test;
    return S.hazard ? RC_SIG_L | RC_SIG_R : S.turn;
}
/* roam_game.c, racecars.c, each frame: another player's car and its signals (RC_SIG_*), paused: their game is (the
 * hazard lights) */
void signals_car(uint32_t car, int sig, int paused){
    if (!car) return;
    int k = -1;
    for (int i = 0; i < NCARS; i++) if (S.o[i].car == car){ k = i; break; }
    if (k < 0) for (int i = 0; i < NCARS; i++) if (!S.o[i].car || S.o[i].seen < S.fr - 1){ k = i; S.o[i].car = 0; break; }
    if (k < 0) return;
    if (S.o[k].car != car){ S.o[k].car = car; S.o[k].sig = 0; S.o[k].paused = 0; }
    sig &= RC_SIG_L | RC_SIG_R; paused = paused != 0;
    if (sig != S.o[k].sig || paused != S.o[k].paused)
        fprintf(stderr, "[signals] f=%ld car 0x%08x: %s%s\n", autotest_frame(), car, paused ? "paused: hazard lights" : sig_name(sig),
                paused && sig ? " (its own signals under them)" : "");
    S.o[k].sig = (uint8_t)sig; S.o[k].paused = (uint8_t)paused; S.o[k].seen = S.fr;
}
/* a car's signals to draw now (RC_SIG_*, 0 none) */
static int car_sig(uint32_t car){
    if (!car) return 0;
    if (car == S.car) return signals_mine();
    for (int i = 0; i < NCARS; i++)
        if (S.o[i].car == car && S.o[i].seen >= S.fr - 1) return S.o[i].paused ? RC_SIG_L | RC_SIG_R : S.o[i].sig;
    return 0;
}

static void press(int what, const char *how){
    if (what == SG_HAZARD) S.hazard = !S.hazard;
    else {
        int s = what == SG_LEFT ? RC_SIG_L : RC_SIG_R;
        S.turn = S.turn == s ? 0 : s;
    }
    fprintf(stderr, "[signals] f=%ld %s: %s%s\n", autotest_frame(), how, sig_name(signals_mine()),
            S.hazard && S.turn ? " (the signal under them stays)" : "");
}
/* the keys' presses since the last frame (none while the player does not drive, none on a key one of the game's
 * controls is on) */
static void key_input(int can){
    static const char *const env[3] = { "NFSU2_KEY_SIGNAL_LEFT", "NFSU2_KEY_SIGNAL_RIGHT", "NFSU2_KEY_HAZARD" };
    static const unsigned def[3] = { DIK_COMMA, DIK_SLASH, DIK_QUOTE };
    static const char *const what[3] = { "the left signal's key", "the right signal's key", "the hazard lights' key" };
    if (!S.dik_set){ S.dik_set = 1; for (int i = 0; i < 3; i++) S.dik[i] = dikbd_site_key(env[i], def[i]); }
    uint32_t head = kbd_ev_head();
    if (!S.kinit){ S.kinit = 1; S.kseq = head; }
    for (; S.kseq != head; S.kseq++){
        KbdEv e;
        int r = kbd_ev_get(S.kseq, &e);
        if (r == 0) break;
        if (r < 0) continue;
        for (int i = 0; i < 3; i++){
            if (!S.dik[i] || e.dik != S.dik[i]) continue;
            int down = e.down != 0;
            if (down == S.kdown[i]) continue;                          /* (a repeat) */
            S.kdown[i] = (uint8_t)down;
            if (down && can && dikbd_ours(S.dik[i], what[i]))
                press(i == 0 ? SG_LEFT : i == 1 ? SG_RIGHT : SG_HAZARD, dik_name(S.dik[i]) ? dik_name(S.dik[i]) : "the key");
        }
    }
}

/* each frame, from the main-loop hook (after roam_game.c's) */
void signals_frame(CPU *c){
    (void)c;
    int can = lightkey_driving();
    key_input(can);
    if (can) pad_signal_offer();
    int p = pad_signal_take();
    if (can && p) press(p, p == SG_LEFT ? "the controller's left signal held" : p == SG_RIGHT ? "the controller's right signal held"
                                                       : "the controller's hazard lights held");
    uint32_t car = my_car();
    if (car != S.car){
        if (S.car && (S.turn || S.hazard)) fprintf(stderr, "[signals] f=%ld another car: signals off\n", autotest_frame());
        S.car = car; S.turn = 0; S.hazard = 0;
    }
    if (guest_rd32(G_FLOW) != 6u){ S.turn = 0; S.hazard = 0; }
    int st = signals_mine();
    if (st != S.last && S.last >= 0 && !p) fprintf(stderr, "[signals] f=%ld mine: %s\n", autotest_frame(), sig_name(st));
    S.last = st;
    S.fr++;
}

/* the first time a car's signals are drawn: which of its lights there are for them */
static void say_lights(uint32_t ri, int n, int front, int rear){
    for (int i = 0; i < S.nsaid; i++) if (S.said[i].ri == ri && S.said[i].n == n) return;
    int k = S.nsaid < 32 ? S.nsaid++ : (int)(S.drawn % 32u);
    S.said[k].ri = ri; S.said[k].n = n;
    fprintf(stderr, "[signals] f=%ld car 0x%08x: %d of its lights for the signals (%d front, %d rear)%s\n", autotest_frame(),
            guest_rd32(ri + RI_CAR), n, front, rear, n ? "" : ": nothing to draw them on");
}

/* photo.c's override of 0x615630, after the game's flares of the car (ri its render info; a its six arguments, as
 * they were at the call) */
void signals_flares(CPU *c, uint32_t ri, const uint32_t *a){
    if (!guest_ok(ri, RI_LIGHTS + 8u)) return;
    uint32_t car = MR32(ri + RI_CAR);
    int sig = car_sig(car);
    if (!sig || fmod(now_s(), SIG_PERIOD_S) >= SIG_ON_S) return;
    if (!S.ring && !(S.ring = gheap_alloc(1, RING * LIGHT_SZ, 1))) return;
    /* the matrix, as 0x615630 makes its own: the car's (a[2]) placed at a[1], from the frame's pool */
    uint32_t m = guest_rd32(G_MPOOL), end = guest_rd32(G_MPOOL_END), at = a[1], mx = a[2];
    if (!m || m + 0x40u >= end){ S.pool_full++; return; }
    if (!guest_ok(at, 11) || !guest_ok(mx, 0x3f)) return;
    MR32(G_MPOOL) = m + 0x40u;
    for (uint32_t i = 0; i < 0x40u; i += 4u) MR32(m + i) = MR32(mx + i);
    MR32(m + 0x30u) = MR32(at); MR32(m + 0x34u) = MR32(at + 4u); MR32(m + 0x38u) = MR32(at + 8u); MRF32(m + 0x3cu) = 1.f;
    float k = 1.f;                                                  /* full strength, times 0x615630's last */
    union { uint32_t u; float f; } s6 = { a[5] };
    if (s6.f != 0.f && s6.f == s6.f) k *= s6.f;
    uint32_t head = ri + RI_LIGHTS, node = guest_rd32(head);
    int n = 0, front = 0, rear = 0;
    for (int guard = 0; node && node != head && guard < 32 && guest_ok(node, LIGHT_SZ - 1u); guard++, node = MR32(node)){
        uint32_t h = MR32(node + LT_HASH);
        int left = h == HASH_HL_L || h == HASH_BL_L, right = h == HASH_HL_R || h == HASH_BL_R, fr = h == HASH_HL_L || h == HASH_HL_R;
        if (!left && !right) continue;
        n++; front += fr; rear += !fr;
        if (!(sig & (left ? RC_SIG_L : RC_SIG_R))) continue;
        uint32_t l = S.ring + LIGHT_SZ * S.ri; S.ri = (S.ri + 1u) % RING;
        for (uint32_t i = 0; i < LIGHT_SZ; i += 4u) MR32(l + i) = MR32(node + i);
        /* further out across the car (its y: + to the left), a headlight's kind */
        float y = MRF32(l + LT_POS + 4u), out = fr ? S.out_f : S.out_r;
        MRF32(l + LT_POS + 4u) = y + (y >= 0.f ? out : -out);
        MR8(l + LT_KIND) = 0;
        /* the view's modes as 0x615630 passes them for its own (0x61596F): none; the visibility by the car's level;
         * or by the light's depth in the view */
        uint32_t v1 = 0, v2 = 0, depth = 0;
        if (a[4]){
            if (guest_ok(car, CAR_LEVEL + 3u) && (int32_t)MR32(car + CAR_LEVEL) < 3){ v1 = 1; v2 = 2; }
            else {
                float x = MRF32(l + LT_POS), yy = MRF32(l + LT_POS + 4u), z = MRF32(l + LT_POS + 8u);
                union { float f; uint32_t u; } d = { x * MRF32(m + 8u) + yy * MRF32(m + 0x18u) - z * MRF32(m + 0x28u) + MRF32(m + 0x38u) };
                v1 = 2; v2 = 2; depth = d.u;
            }
        }
        union { float f; uint32_t u; } sk = { k };
        uint32_t bgr = (S.colour >> 16 & 0xffu) | (S.colour & 0xff00u) | (S.colour & 0xffu) << 16;   /* RRGGBB -> BBGGRR */
        uint32_t args[8] = { a[0], l, m, sk.u, v1, v2, depth, bgr };
        gcall(c, FN_FLARE, 0, 8, args);
        S.drawn++;
    }
    say_lights(ri, n, front, rear);
}

/* the test script's "signals ...": left|right|hazard|off (ours, the keys' again with "keys"), look OUT_F OUT_R RRGGBB,
 * report */
int signals_script(const char *s){
    char w[16] = {0};
    float f = 0, r = 0; unsigned col = 0;
    if (sscanf(s, "%15s", w) != 1) return 0;
    if (!strcasecmp(w, "left")) S.test = RC_SIG_L;
    else if (!strcasecmp(w, "right")) S.test = RC_SIG_R;
    else if (!strcasecmp(w, "hazard")) S.test = RC_SIG_L | RC_SIG_R;
    else if (!strcasecmp(w, "off")) S.test = 0;
    else if (!strcasecmp(w, "keys")) S.test = -1;
    else if (!strcasecmp(w, "look") && sscanf(s, "%*s %f %f %x", &f, &r, &col) == 3){ S.out_f = f; S.out_r = r; S.colour = col & 0xffffffu; }
    else if (!strcasecmp(w, "report")){
        int others = 0;
        for (int i = 0; i < NCARS; i++) others += S.o[i].car && S.o[i].seen >= S.fr - 1;
        fprintf(stderr, "[signals] f=%ld mine %s (car 0x%08x), %d other players' cars, %lu flares drawn, matrix pool full %lu times\n",
                autotest_frame(), sig_name(signals_mine()), S.car, others, S.drawn, S.pool_full);
    } else return 0;
    return 1;
}

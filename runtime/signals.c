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
 * The GPS's signal (NFSU2_GPS_SIGNALS, on unless off): in free roam, with the GPS on a route, our car signals by itself
 * before a turn on it, under the player's own (a key or hold sets theirs over it; a press of its side's key silences
 * it till that turn is past).  The route is the GPS's road search (navigator 0x81CBC0, started for the player's car
 * by 0x5294F8 -> 0x413120) as it hands it to the car's road follower (car +0x2C; its vtable 0x785230, +0x4C =
 * 0x41EA30 keeps the search's last node at +0x3F0): +0x4A9 (s8) a count, +0x428 that many u16, each a joint of the
 * traffic's roads (walls.c: 0x883DB0, 0x20 bytes, the two roads +8/+0xA and their nodes +4/+6) in its low 13 bits,
 * 0x2000 the one the route takes at a fork, clear the others there (0x41AD40; 0x418960 reads it).  The taken joints in
 * order, joined along their roads (each road's nodes from where the route came onto it to the joint off it; from the
 * last joint toward the destination), make the route's line.  The GPS finds its route anew now and then from where
 * the car is (state 0 meanwhile, no route), and a route found inside a junction starts with joints a few metres
 * apart that zigzag (a player's log, 2026-11-16).  A road goes on through junctions and round their corners, so a
 * turn is often no joint at all (a player's 1.4 km route: 4 joints, its turns mostly a road's own corners): the
 * junctions are the taken joints' ends and the roads' nodes with a joint, those closer than GPS_GROUP along the line
 * one junction, and a junction is a turn when the line's way over GPS_LOOK m after it is GPS_TURN_DEG or more off its
 * way over GPS_LOOK m before it; its side, the car's frame's (x forward, y left, z up: a cross product > 0 is a left
 * turn).  The signal comes on GPS_ON_M before the turn along the line (more at speed: GPS_ON_S of driving) and goes
 * off past it, once the car points the line's way out (GPS_DONE_DEG) or GPS_HOLD_S after; the turn is kept by its
 * place: a new route meanwhile keeps it while it has a turn there, else it goes off.
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
#include "setting.h"
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
/* the GPS's signal */
#define G_GPS        0x00838590u   /* the GPS (thiscall's this) */
#define FN_GPS_STATE 0x00501100u   /* GPS::GetState: 0 searching, 3 on a route, 1/2 no satellite or path, 4 off */
#define CAR_FOLLOW   0x2cu         /* a car: its road follower (+4 the car) */
#define CAR_PLACE    0x60u         /* a car: float x, y, z */
#define CAR_FWD      0x90u         /* a car: its forward, float x, y, z (walls.c) */
#define FL_ROUTE_N   0x4a9u        /* the road follower: the route's count (s8, 63 at most) */
#define FL_ROUTE     0x428u        /* ... its joints, u16 each */
#define RT_JOINT     0x1fffu
#define RT_TAKEN     0x2000u
#define G_ROADS      0x0088a334u   /* the traffic's roads (walls.c): 2048 */
#define NROADS       2048u
#define RD_ID        0x0au         /*   s16: the road's own index */
#define RD_NODES     0x34u         /*   s16: its nodes' count */
#define RD_NODE0     0x8cu         /*   its nodes, NODE_SZ each: float x, y */
#define NODE_SZ      0x38u
#define ND_JOINT     0x1cu         /*   u16 x 2: the joints at the node (0xFFFF none) */
#define G_JOINTS     0x00883db0u   /* the joints, JOINT_SZ each: +4/+6 the two nodes, +8/+0xA their roads (s16) */
#define G_NJOINTS    0x00883db4u
#define JOINT_SZ     0x20u
#define GPS_TURN_DEG 40.f          /* a turn: the route's way out this much off its way in */
#define GPS_LOOK     25.f          /* m: the way in, over this much of the route before the junction; the way out, after */
#define GPS_GROUP    30.f          /* m: junction points closer than this along the route are one junction, ... */
#define GPS_GROUP_MAX 45.f         /* ... over this much of it at most */
#define GPS_SAME_M   20.f          /* m: a turn this close to another is the same one (the GPS finds its route anew now and then) */
#define GPS_PTS      2048          /* the route's line: points at most */
#define GPS_NEAR_LINE 400.f        /* m: the car is looked for on the route's line this far on from where it last was
                                      (the GPS finds its route anew only now and then: 35 s apart in a player's drive), ... */
#define GPS_OFF_LINE 40.f          /* ... and on all of it when that is farther than this */
#define GPS_CLOSE_M  30.f          /* m: a turn is past once the car came this near it and has it behind */
#define GPS_ON_M     30.f          /* m along the route before the turn the signal comes on, ... */
#define GPS_ON_S     2.f           /* ... or this long before it at the car's speed, whichever is farther (a player: 3 s and
                                      60 m in a straight line came on seconds early) */
#define GPS_DONE_DEG 20.f          /* past the turn: off once the car points the road out's way this closely, ... */
#define GPS_HOLD_S   4.f           /* ... or this long after */
#define GPS_TURNS    63

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
    struct {                          /* the GPS's signal */
        int on;                       /* -1 not read yet, 0 off (NFSU2_GPS_SIGNALS), 1 on */
        uint16_t route[GPS_TURNS]; int nroute;     /* the route as last read */
        struct turn { float x, y, ox, oy, deg, s; int side; } t[GPS_TURNS], cur;   /* its turns: where, the way out, how much,
                                                                                   RC_SIG_*; cur: the one signalled (side 0: none) */
        int nt, sig;                  /* the turns' count; signalling now (RC_SIG_*, 0) */
        int mute; float mute_x, mute_y;   /* the player silenced the turn there */
        float done_x, done_y;         /* the last turn past */
        double past;                  /* when the car went past cur (0: not yet) */
        float px, py; double pt;      /* the car's place a moment ago, for its speed */
        float sc;                     /* the car's place along the route's line, as last found */
        float closest;                /* how near the car came to cur */
        float speed;
    } g;
    unsigned long drawn, pool_full;
} S = { .test = -1, .last = -1, .out_f = 0.15f, .out_r = 0.06f, .colour = 0xff8c00u, .g = { .on = -1, .done_x = 1e9f, .done_y = 1e9f } };

static double now_s(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec + ts.tv_nsec * 1e-9; }
static uint32_t my_car(void){ uint32_t pl = guest_rd32(G_PLAYER1); return pl ? guest_rd32(pl + 4u) : 0u; }
static const char *sig_name(int s){
    return (s & 3) == 3 ? "hazard lights" : s & RC_SIG_L ? "left signal" : s & RC_SIG_R ? "right signal" : "off";
}

/* ours now: RC_SIG_L, RC_SIG_R, both (the hazard lights) or 0 */
int signals_mine(void){
    if (S.test >= 0) return S.test;
    return S.hazard ? RC_SIG_L | RC_SIG_R : S.turn ? S.turn : S.g.sig;
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
        if (!S.hazard && !S.turn && S.g.sig == s){             /* the GPS's own signal: silenced till that turn is past */
            S.g.mute = 1; S.g.mute_x = S.g.cur.x; S.g.mute_y = S.g.cur.y; S.g.sig = 0;
            fprintf(stderr, "[signals] f=%ld %s: the GPS's %s off for this turn\n", autotest_frame(), how, sig_name(s));
            S.last = signals_mine();
            return;
        }
        S.turn = S.turn == s ? 0 : s;
    }
    fprintf(stderr, "[signals] f=%ld %s: %s%s\n", autotest_frame(), how, sig_name(signals_mine()),
            S.hazard && S.turn ? " (the signal under them stays)" : "");
    S.last = signals_mine();                                      /* (said: no "mine" line for it) */
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

/* ---- the GPS's signal ---- */
static uint32_t gps_road(int i){
    if (i < 0 || i >= (int)NROADS) return 0;
    uint32_t r = guest_rd32(G_ROADS + 4u * (uint32_t)i);
    if (!guest_ok(r, RD_NODE0) || (int16_t)MR16(r + RD_ID) != i) return 0;
    int n = (int16_t)MR16(r + RD_NODES);
    return n > 0 && n < 4096 && guest_ok(r + RD_NODE0, (uint32_t)n * NODE_SZ) ? r : 0;
}
static int road_nodes(uint32_t r){ return (int16_t)MR16(r + RD_NODES); }
static void node_xy(uint32_t r, int k, float *x, float *y){
    uint32_t nd = r + RD_NODE0 + NODE_SZ * (uint32_t)k;
    *x = MRF32(nd); *y = MRF32(nd + 4u);
}
/* a joint's two ends (road, node), 0 when it is not one */
static int joint_ends(uint32_t j, int *ra, int *na, int *rb, int *nb){
    uint32_t nj = guest_rd32(G_NJOINTS), jt = guest_rd32(G_JOINTS);
    if (j >= nj || nj > 65536u || !guest_ok(jt + JOINT_SZ * j, JOINT_SZ)) return 0;
    uint32_t e = jt + JOINT_SZ * j;
    *na = (int16_t)MR16(e + 4u); *nb = (int16_t)MR16(e + 6u); *ra = (int16_t)MR16(e + 8u); *rb = (int16_t)MR16(e + 0xau);
    uint32_t a = gps_road(*ra), b = gps_road(*rb);
    return a && b && *na >= 0 && *nb >= 0 && *na < road_nodes(a) && *nb < road_nodes(b);
}
/* the route as a line: the car, then along each road the route takes (its nodes) to the joint off it, the joint's
 * ends, and from the last joint along its road toward the destination, then the destination.  jn: the point is at a
 * junction (a taken joint's end, or a road's node with a joint: +0x1C/+0x1E, 0xFFFF none, as the road search reads
 * them, 0x5D4380). */
static struct { float x[GPS_PTS], y[GPS_PTS], s[GPS_PTS]; uint8_t jn[GPS_PTS]; int n; } L;
static void pt(float x, float y, int jn){
    if (L.n >= GPS_PTS || !isfinite(x) || !isfinite(y)) return;
    if (L.n && fabsf(x - L.x[L.n - 1]) < 0.01f && fabsf(y - L.y[L.n - 1]) < 0.01f){ L.jn[L.n - 1] |= (uint8_t)jn; return; }
    L.x[L.n] = x; L.y[L.n] = y; L.s[L.n] = L.n ? L.s[L.n - 1] + hypotf(x - L.x[L.n - 1], y - L.y[L.n - 1]) : 0.f;
    L.jn[L.n] = (uint8_t)jn; L.n++;
}
static int node_junction(uint32_t r, int k){
    uint32_t nd = r + RD_NODE0 + NODE_SZ * (uint32_t)k, nj = guest_rd32(G_NJOINTS);
    uint32_t a = MR16(nd + ND_JOINT), b = MR16(nd + ND_JOINT + 2u);
    return (a != 0xffffu && a < nj) || (b != 0xffffu && b < nj);
}
/* road ri's nodes from k0 to k1, both in */
static void along(int ri, int k0, int k1){
    uint32_t r = gps_road(ri);
    if (!r) return;
    int n = road_nodes(r), st = k1 >= k0 ? 1 : -1;
    for (int k = k0, g = 0; k >= 0 && k < n && g < 4096; k += st, g++){
        float x, y; node_xy(r, k, &x, &y); pt(x, y, node_junction(r, k));
        if (k == k1) break;
    }
}
/* the place on the line at distance d along it (clamped), and the line's way there */
static void at(float d, float *x, float *y){
    if (!L.n){ *x = *y = 0; return; }
    if (d <= 0.f){ *x = L.x[0]; *y = L.y[0]; return; }
    for (int i = 1; i < L.n; i++) if (L.s[i] >= d){
        float u = L.s[i] > L.s[i - 1] ? (d - L.s[i - 1]) / (L.s[i] - L.s[i - 1]) : 0.f;
        *x = L.x[i - 1] + u * (L.x[i] - L.x[i - 1]); *y = L.y[i - 1] + u * (L.y[i] - L.y[i - 1]); return;
    }
    *x = L.x[L.n - 1]; *y = L.y[L.n - 1];
}
static int way(float d0, float d1, float *dx, float *dy){
    float x0, y0, x1, y1;
    at(d0, &x0, &y0); at(d1, &x1, &y1);
    float l = hypotf(x1 - x0, y1 - y0);
    if (!(l > 2.f)) return 0;
    *dx = (x1 - x0) / l; *dy = (y1 - y0) / l;
    return 1;
}
/* the route's turns: its line, then each group of taken joints (closer than GPS_GROUP along it: one junction) where
 * the way GPS_LOOK before the group and GPS_LOOK after it differ by GPS_TURN_DEG or more */
static void gps_turns(float cx, float cy){
    S.g.nt = 0; L.n = 0;
    int tk[GPS_TURNS], n = 0;
    for (int i = 0; i < S.g.nroute; i++) if (S.g.route[i] & RT_TAKEN) tk[n++] = S.g.route[i] & RT_JOINT;
    int pr = -1, pn = -1;
    pt(cx, cy, 0);
    for (int i = 0; i < n; i++){
        int ra, na, rb, nb;
        if (!joint_ends((uint32_t)tk[i], &ra, &na, &rb, &nb)) continue;
        int flip;                                                 /* its first end: on the road the route came along */
        if (pr >= 0) flip = rb == pr && ra != pr;
        else {
            float ax, ay, bx, by;
            node_xy(gps_road(ra), na, &ax, &ay); node_xy(gps_road(rb), nb, &bx, &by);
            flip = hypotf(bx - cx, by - cy) < hypotf(ax - cx, ay - cy);
        }
        if (flip){ int t = ra; ra = rb; rb = t; t = na; na = nb; nb = t; }
        if (pr == ra && pn >= 0) along(ra, pn, na);
        else if (pr < 0){                                         /* the first: from the car's nearest node of that road */
            uint32_t r = gps_road(ra); int best = na; float bd = 1e30f;
            for (int k = 0; r && k < road_nodes(r); k++){ float x, y; node_xy(r, k, &x, &y); float d = hypotf(x - cx, y - cy); if (d < bd){ bd = d; best = k; } }
            along(ra, best, na);
        }
        float x, y;
        node_xy(gps_road(ra), na, &x, &y); pt(x, y, 1);
        node_xy(gps_road(rb), nb, &x, &y); pt(x, y, 1);
        pr = rb; pn = nb;
    }
    if (pr >= 0){                                                 /* on toward the destination */
        uint32_t r = gps_road(pr);
        float gx = MRF32(G_GPS + 0x10u), gy = MRF32(G_GPS + 0x14u), x, y;
        if (r && isfinite(gx) && isfinite(gy)){
            int m = road_nodes(r), best = pn; float bd = 1e30f;
            for (int k = 0; k < m; k++){ node_xy(r, k, &x, &y); float d = hypotf(x - gx, y - gy); if (d < bd){ bd = d; best = k; } }
            along(pr, pn, best);
            pt(gx, gy, 0);
        }
    }
    for (int i = 0; i < L.n; ){
        if (!L.jn[i]){ i++; continue; }
        int j = i;                                                /* the junction's points: closer than GPS_GROUP, ... */
        for (int k = i + 1; k < L.n && L.s[k] - L.s[j] < GPS_GROUP && L.s[k] - L.s[i] < GPS_GROUP_MAX; k++) if (L.jn[k]) j = k;
        float s0 = L.s[i], s1 = L.s[j], ix, iy, ox, oy;
        i = j + 1;
        if (!way(s0 - GPS_LOOK, s0, &ix, &iy) || !way(s1, s1 + GPS_LOOK, &ox, &oy)) continue;
        float deg = atan2f(ix * oy - iy * ox, ix * ox + iy * oy) * (180.f / 3.14159265f);
        if (fabsf(deg) < GPS_TURN_DEG || S.g.nt >= GPS_TURNS) continue;
        float x, y; at(s0, &x, &y);
        S.g.t[S.g.nt].x = x; S.g.t[S.g.nt].y = y; S.g.t[S.g.nt].ox = ox; S.g.t[S.g.nt].oy = oy;
        S.g.t[S.g.nt].side = deg > 0.f ? RC_SIG_L : RC_SIG_R; S.g.t[S.g.nt].deg = deg; S.g.t[S.g.nt].s = s0;
        S.g.nt++;
    }
    fprintf(stderr, "[signals] f=%ld GPS route: %d entries, %d joints taken, a line of %d points, %.0f m; %d turns; the car at %.0f %.0f\n",
            autotest_frame(), S.g.nroute, n, L.n, L.n ? L.s[L.n - 1] : 0.f, S.g.nt, cx, cy);
    for (int i = 0; i < S.g.nt; i++)
        fprintf(stderr, "[signals]   turn %d: %s (%.0f deg) at %.0f %.0f\n", i, sig_name(S.g.t[i].side), S.g.t[i].deg, S.g.t[i].x, S.g.t[i].y);
}
static void gps_off(void){ S.g.sig = 0; S.g.cur.side = 0; S.g.past = 0; }
static int near_xy(float ax, float ay, float bx, float by){ return hypotf(ax - bx, ay - by) < GPS_SAME_M; }
/* each frame: the GPS's signal now (S.g.sig) */
static void gps_signal(CPU *c, int can, uint32_t car){
    if (S.g.on < 0) S.g.on = setting_bool("NFSU2_GPS_SIGNALS", 1);
    if (!S.g.on || !can || !car || guest_rd32(G_RACETYPE) != 0u || !guest_ok(car, CAR_FWD + 12u)){
        S.g.nroute = 0; S.g.nt = 0; gps_off(); return;
    }
    uint32_t st = gcall(c, FN_GPS_STATE, G_GPS, 0, NULL);
    if (st != 0u && st != 3u){ S.g.nroute = 0; S.g.nt = 0; gps_off(); return; }   /* no route: off */
    uint32_t fl = MR32(car + CAR_FOLLOW);
    int n = st == 3u && guest_ok(fl, FL_ROUTE_N) && MR32(fl + 4u) == car ? (int8_t)MR8(fl + FL_ROUTE_N) : 0;
    if (n < 0 || n > GPS_TURNS) n = 0;
    float cx = MRF32(car + CAR_PLACE), cy = MRF32(car + CAR_PLACE + 4u);
    double now = now_s();
    if (S.g.pt > 0 && now > S.g.pt){
        float v = hypotf(cx - S.g.px, cy - S.g.py) / (float)(now - S.g.pt);
        if (isfinite(v) && v < 150.f) S.g.speed = S.g.speed * 0.8f + v * 0.2f;
    }
    S.g.px = cx; S.g.py = cy; S.g.pt = now;
    /* the GPS finds its route again now and then from where the car is (meanwhile none, state 0): the last one
     * stays till the new one is in */
    int changed = n > 0 && n != S.g.nroute;
    for (int i = 0; i < n && !changed; i++) changed = MR16(fl + FL_ROUTE + 2u * (uint32_t)i) != S.g.route[i];
    if (changed){
        S.g.nroute = n;
        for (int i = 0; i < n; i++) S.g.route[i] = MR16(fl + FL_ROUTE + 2u * (uint32_t)i);
        gps_turns(cx, cy);
        S.g.sc = 0.f;
        int kept = 0;
        for (int i = 0; i < S.g.nt && S.g.cur.side && !kept; i++)
            kept = S.g.t[i].side == S.g.cur.side && near_xy(S.g.t[i].x, S.g.t[i].y, S.g.cur.x, S.g.cur.y);
        if (S.g.cur.side && !S.g.past && !kept){                 /* a sharper route has the turn elsewhere, or none */
            fprintf(stderr, "[signals] f=%ld GPS: the new route has no turn there, signal off\n", autotest_frame());
            gps_off();
        }
    }
    float fx = MRF32(car + CAR_FWD), fy = MRF32(car + CAR_FWD + 4u), fl2 = hypotf(fx, fy);
    if (!(fl2 > 0.1f)) return;
    fx /= fl2; fy /= fl2;
    float reach = fmaxf(GPS_ON_M, S.g.speed * GPS_ON_S);
    /* the turn being signalled: off once it is past (or the car went another way) */
    if (S.g.cur.side){
        float dx = S.g.cur.x - cx, dy = S.g.cur.y - cy, d = hypotf(dx, dy);
        if (d < S.g.closest) S.g.closest = d;
        if (dx * fx + dy * fy < 0.f && !S.g.past && S.g.closest < GPS_CLOSE_M) S.g.past = now;
        float a = acosf(fminf(1.f, fx * S.g.cur.ox + fy * S.g.cur.oy)) * (180.f / 3.14159265f);
        if (S.g.past ? (a < GPS_DONE_DEG && d > GPS_SAME_M) || now - S.g.past > GPS_HOLD_S || d > GPS_ON_M : d > reach + 30.f){
            fprintf(stderr, "[signals] f=%ld GPS: past the turn, signal off\n", autotest_frame());
            S.g.done_x = S.g.cur.x; S.g.done_y = S.g.cur.y;
            if (S.g.mute && near_xy(S.g.mute_x, S.g.mute_y, S.g.cur.x, S.g.cur.y)) S.g.mute = 0;
            gps_off();
        }
        return;
    }
    /* the next turn ahead within reach along the route (not the one just past, which a new route may still have under
     * the car): the car's place on the route's line, the nearest within its first GPS_NEAR_LINE m */
    float sc = S.g.sc, bd = 1e30f;
    for (int pass = 0; pass < 2 && !(bd < GPS_OFF_LINE); pass++){
        float lo = pass ? -1.f : S.g.sc - 50.f, hi = pass ? 1e30f : S.g.sc + GPS_NEAR_LINE;
        for (int i = 1; i < L.n; i++){
            if (L.s[i] < lo || L.s[i - 1] > hi) continue;
            float ax = L.x[i - 1], ay = L.y[i - 1], bx = L.x[i] - ax, by = L.y[i] - ay, l2 = bx * bx + by * by;
            float u = l2 > 1e-6f ? ((cx - ax) * bx + (cy - ay) * by) / l2 : 0.f;
            u = u < 0.f ? 0.f : u > 1.f ? 1.f : u;
            float d = hypotf(ax + u * bx - cx, ay + u * by - cy);
            if (d < bd){ bd = d; sc = L.s[i - 1] + u * (L.s[i] - L.s[i - 1]); }
        }
    }
    S.g.sc = sc;
    for (int i = 0; i < S.g.nt; i++){
        float d = S.g.t[i].s - sc;
        if (d > reach || d <= 0.f || near_xy(S.g.t[i].x, S.g.t[i].y, S.g.done_x, S.g.done_y)) continue;
        S.g.cur = S.g.t[i]; S.g.past = 0; S.g.closest = 1e30f;
        S.g.sig = S.g.mute && near_xy(S.g.mute_x, S.g.mute_y, S.g.cur.x, S.g.cur.y) ? 0 : S.g.cur.side;
        fprintf(stderr, "[signals] f=%ld GPS: %s, a turn of %.0f deg in %.0f m%s\n", autotest_frame(), sig_name(S.g.cur.side),
                S.g.cur.deg, d, S.g.sig ? "" : " (silenced)");
        return;
    }
}

/* each frame, from the main-loop hook (after roam_game.c's) */
void signals_frame(CPU *c){
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
    gps_signal(c, can && guest_rd32(G_FLOW) == 6u, car);
    int st = signals_mine();
    if (st != S.last && S.last >= 0) fprintf(stderr, "[signals] f=%ld mine: %s\n", autotest_frame(), sig_name(st));
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

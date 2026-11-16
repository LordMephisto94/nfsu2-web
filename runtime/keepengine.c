/* keepengine.c -- Keep engine sounds (NFSU2_KEEP_ENGINE, off unless on): a car's engine upgrades no longer swap its
 * engine sound.
 *
 * The game picks a car's engine sound when its sound state is set up (0x4593B0, thiscall on the state, the car's
 * ride its argument; from 0x4698A8): the sound type of its car type (0x459240) at +0x8C, and a level from its engine
 * upgrades (the ride's performance at +0x300, 0x599610: package 0x18 installed 3, 0x13 2, 0xF 1, else 0) at +0x94 and
 * +0x90; then the entry of the table 0x7F2230 for both (12 bytes, sound type x 4 + level) at +0x88, its engine (the
 * entry's first dword) at +0x7C and that engine's record (0x7F0DE0, 0x4C each) at +0x80; and, when the record has a
 * bank (+0x10) outside a split-screen race (front-end mode 4), +0x78 = 1 and the letting-off recording (0x7F1AA8, 0x34
 * each, the record's +0x14) at +0x84, else +0x78 = 0 and +0x84 the table's first.  It only picks: the sound is loaded
 * and played from these afterwards.  With the setting on, the same is picked again at level 0, the stock engine's:
 * every car sounds as it does with no engine upgrades, the player's, AI racers' and friends' alike (roam_game.c
 * roam_game_engine_files picks a friend's car's the same way).  Log: "[keepengine]" once a car's sound is kept. */
#include "cpu.h"
#include "guest.h"
#include "game.h"
#include "setting.h"
#include <stdio.h>

void sub_004593b0_orig(CPU *c);

#define SND_LEVEL     0x94u       /* the sound state: the engine upgrades' level (0 .. 3) */
#define SND_LEVEL2    0x90u       /*   ...again, the one the table is read with */
#define SND_TYPE      0x8cu       /*   the car type's sound type (0 .. 30) */
#define SND_PICK      0x88u       /*   the table's entry */
#define SND_ENGINE    0x7cu       /*   its engine */
#define SND_RECORD    0x80u       /*   the engine's record */
#define SND_HASBANK   0x78u       /*   1: the record has a bank */
#define SND_DCL       0x84u       /*   the letting-off recording */
#define G_ENG_PICK    0x007f2230u
#define G_ENGINES     0x007f0de0u
#define N_ENGINES     43u
#define G_ENG_DCL     0x007f1aa8u
#define N_ENG_DCL     37u

static int keepengine_on(void){
    static int on = -1;
    if (on < 0) on = setting_bool("NFSU2_KEEP_ENGINE", 0);
    return on;
}

/* the engine pick (0x4593B0), thiscall, ret 4 */
void sub_004593b0(CPU *c){
    uint32_t self = c->ecx;
    sub_004593b0_orig(c);
    if (!keepengine_on() || !guest_ok(self, SND_LEVEL + 3u)) return;
    uint32_t lvl = MR32(self + SND_LEVEL), st = MR32(self + SND_TYPE);
    if (!lvl || st > 30u) return;
    uint32_t pick = G_ENG_PICK + 12u * (st * 4u), e = MR32(pick), rec = G_ENGINES + 0x4cu * e;
    if (e >= N_ENGINES) return;
    MR32(self + SND_LEVEL) = 0; MR32(self + SND_LEVEL2) = 0;
    MR32(self + SND_PICK) = pick; MR32(self + SND_ENGINE) = e; MR32(self + SND_RECORD) = rec;
    uint32_t d = MR32(rec + 0x14u);
    if (MR32(rec + 0x10u) && MR32(G_GAMEMODE) != 4u && d < N_ENG_DCL){
        MR32(self + SND_HASBANK) = 1; MR32(self + SND_DCL) = G_ENG_DCL + 0x34u * d;
    } else {
        MR32(self + SND_HASBANK) = 0; MR32(self + SND_DCL) = G_ENG_DCL;
    }
    fprintf(stderr, "[keepengine] sound type %u: its stock engine %u, not its level %u upgrade's %u\n", st, e, lvl,
            MR32(G_ENG_PICK + 12u * (st * 4u + (lvl < 4u ? lvl : 0u))));
}

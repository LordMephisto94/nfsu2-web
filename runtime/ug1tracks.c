/* ug1tracks.c -- Underground 1 Tracks (NFSU2_UG1_TRACKS, on unless off): RKBDI's Olympic City of the first
 * Underground, its races and the demo's, as more tracks of the game's own.
 *
 * The pack is the game folder's MODPACKS/UG1 (as nfsu2.xyz ships it): six regions of their own (TRACKS/L4RO the city,
 * L4RU Downtown and Mission, L4RV Highway 1, L4RW Commercial, L4JA and L4JB the demo's), each its region file, its
 * stream and its routes, as the game's own regions have them, and their TRACKMAP pictures; the game reads them as its
 * own files (fileio.c: a file the game folder has not, from MODPACKS/UG1/<its path> while this is on).
 * UG1TRACKS.DAT is their tracks and names:
 *   +0   'U1TK', version 1, the header's size (32), the tracks' count, a track's size (0x128: the game's TrackInfo),
 *        the names' count, where the names are, where their text is;
 *   the tracks: TrackInfo records as the game's own track table has them (GLOBALB's chunk 0x34201: +0 its name, +0x40
 *        its region, +0x8A its number, +0x94 its flags), numbered 5xxx: 5002, 5004, 5005, 5006, 5500 and 5700 each
 *        region's free roam, the others its races;
 *   the names: { the text's name's hash (bStringHash), where its text starts after the text's start, 0 }, 12 bytes
 *        each: TRACK_NAME_<n>'s and their reverse's, the text Latin-1, each ended by a 0.
 * The game's track table is only ever read through its two globals (0x883DA0 the records, 0x883DA4 their count; by
 * number 0x5D3E40 and 0x5E92DE, by index 0x5D3E80, Quick Race's list 0x4CDED0), which its chunk's loader sets
 * (0x5D3EB0, from the chunk loaders' table): after it, they point at a copy of the game's records with the pack's
 * after them.  The names go into the game's language table (language.c).  The trips there are the page's Tracks
 * (travel.c's UG1 spots).  Log: "[ug1]". */
#include "cpu.h"
#include "guest.h"
#include "game.h"
#include "setting.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void sub_005d3eb0_orig(CPU *c);
int fileio_mod_pack(const char *rel, char *out, size_t outsz);                 /* fileio.c */
uint32_t gheap_alloc(int heap, uint32_t n, int zero);                           /* heap.c */
uint32_t language_add_text_hash(uint32_t hash, const char *text);              /* language.c */

#define G_TRACKS     0x00883da0u   /* the track table: its records, ... */
#define G_NTRACKS    0x00883da4u   /* ... and their count */
#define TRACK_SZ     0x128u
#define TI_NUMBER    0x8au         /* s16: the track's number */
#define DAT          "UG1/UG1TRACKS.DAT"
#define MAX_TRACKS   256u
#define MAX_NAMES    512u

static struct {
    int read, ok;                 /* the pack read (once); usable */
    uint8_t *rec; uint32_t n;     /* its tracks */
    uint32_t loaded_from, table, count;   /* the game's chunk the copy was made for, the copy, its count */
} U;

/* on, and the server has the pack */
int ug1_on(void){
    static int on = -1;
    if (on < 0){
        char p[1024];
        on = setting_bool("NFSU2_UG1_TRACKS", 1) && fileio_mod_pack(DAT, p, sizeof p);
        if (setting_bool("NFSU2_UG1_TRACKS", 1) && !on) fprintf(stderr, "[ug1] this server has not the pack (MODPACKS/%s): no UG1 tracks\n", DAT);
    }
    return on;
}

static uint32_t rd(const uint8_t *b){ return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }

static void read_pack(void){
    if (U.read) return;
    U.read = 1;
    char p[1024];
    if (!ug1_on() || !fileio_mod_pack(DAT, p, sizeof p)) return;
    FILE *f = fopen(p, "rb");
    if (!f){ fprintf(stderr, "[ug1] %s: not read\n", DAT); return; }
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *d = len > 32 && len < (1 << 22) ? malloc((size_t)len) : 0;
    if (!d || fread(d, 1, (size_t)len, f) != (size_t)len){ fclose(f); free(d); fprintf(stderr, "[ug1] %s: not read\n", DAT); return; }
    fclose(f);
    uint32_t hdr = rd(d + 8), n = rd(d + 12), sz = rd(d + 16), nn = rd(d + 20), names = rd(d + 24), text = rd(d + 28);
    if (memcmp(d, "U1TK", 4) || rd(d + 4) != 1u || sz != TRACK_SZ || n > MAX_TRACKS || nn > MAX_NAMES ||
        (uint64_t)hdr + (uint64_t)n * sz > (uint64_t)len || (uint64_t)names + 12u * (uint64_t)nn > (uint64_t)len || text > (uint32_t)len){
        fprintf(stderr, "[ug1] %s: not a pack this reads\n", DAT); free(d); return;
    }
    U.rec = malloc((size_t)n * TRACK_SZ);
    if (!U.rec){ free(d); return; }
    memcpy(U.rec, d + hdr, (size_t)n * TRACK_SZ); U.n = n;
    int named = 0;
    for (uint32_t i = 0; i < nn; i++){
        const uint8_t *e = d + names + 12u * i;
        uint32_t at = text + rd(e + 4);
        if (at >= (uint32_t)len || !memchr(d + at, 0, (size_t)len - at)) continue;
        language_add_text_hash(rd(e), (const char *)d + at);
        named++;
    }
    free(d);
    U.ok = 1;
    fprintf(stderr, "[ug1] %u tracks, %d names\n", U.n, named);
}

/* the track table's chunk loaded (GLOBALB's 0x34201): the game's records and the pack's after them */
void sub_005d3eb0(CPU *c){
    uint32_t chunk = MR32(c->esp + 4u);
    sub_005d3eb0_orig(c);
    if (!c->eax || !ug1_on()) return;
    read_pack();
    uint32_t t = MR32(G_TRACKS), n = MR32(G_NTRACKS);
    if (!U.ok || !t || n > 4096u || !guest_ok_bytes(t, n * TRACK_SZ)) return;
    if (U.loaded_from != chunk || !U.table){
        uint32_t m = gheap_alloc(1, (n + U.n) * TRACK_SZ, 1);
        if (!m){ fprintf(stderr, "[ug1] no guest memory for the track table\n"); return; }
        memcpy(g_mem + m, g_mem + t, (size_t)n * TRACK_SZ);
        uint32_t added = 0;
        for (uint32_t i = 0; i < U.n; i++){
            int16_t num = (int16_t)(U.rec[TRACK_SZ * i + TI_NUMBER] | U.rec[TRACK_SZ * i + TI_NUMBER + 1] << 8);
            int dup = 0;
            for (uint32_t j = 0; j < n && !dup; j++) dup = (int16_t)MR16(t + TRACK_SZ * j + TI_NUMBER) == num;
            if (dup) continue;                                     /* (a track the game has keeps the game's) */
            memcpy(g_mem + m + TRACK_SZ * (n + added), U.rec + TRACK_SZ * i, TRACK_SZ);
            added++;
        }
        U.table = m; U.loaded_from = chunk; U.count = n + added;
        MR32(G_TRACKS) = m; MR32(G_NTRACKS) = n + added;
        fprintf(stderr, "[ug1] the track table: the game's %u and %u of the pack's\n", n, added);
        return;
    }
    MR32(G_TRACKS) = U.table;                                      /* (the same chunk again: the copy made for it) */
    MR32(G_NTRACKS) = U.count;
}

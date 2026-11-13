/* Shared split-screen AI configuration rules. No game memory or host dependencies. */
#ifndef NFSU2_SPLITSCREEN_AI_H
#define NFSU2_SPLITSCREEN_AI_H
static inline unsigned split_ai_parse_count(const char *s){
    if (!s || !*s) return 0;
    unsigned n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p){
        if (*p < '0' || *p > '9') return 0;
        if (n < 6){
            n = n * 10 + (*p - '0');
            if (n > 5) n = 6;
        }
    }
    return n > 5 ? 5 : n;
}
static inline int split_ai_supported_mode(unsigned type){
    switch (type){
    case 0: case 1: case 2: case 4: case 5: case 7: case 10: return 1;
    default: return 0; /* Drift needs circuit-vs-sprint metadata. */
    }
}
/* Upstream Opponents row: left/right wrap through 0..(6 - humans).
 * Pure helper so the guest menu callback can use identical rules. */
static inline unsigned split_ai_step(unsigned selected, unsigned humans, int direction){
    if (humans < 2 || humans >= 6) return 0;
    unsigned max = 6 - humans;
    if (selected > max) selected = max;
    if (direction < 0) return selected ? selected - 1 : max;
    if (direction > 0) return selected < max ? selected + 1 : 0;
    return selected;
}
static inline unsigned split_ai_cap(unsigned requested, unsigned humans){
    if (humans < 2 || humans >= 6) return 0;
    unsigned slots = 6 - humans;
    return requested < slots ? requested : slots;
}
#endif

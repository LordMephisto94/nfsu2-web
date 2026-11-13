#include <assert.h>
#include "../runtime/splitscreen_ai.h"
int main(void){
    assert(split_ai_parse_count(0) == 0);
    assert(split_ai_parse_count("") == 0);
    assert(split_ai_parse_count("0") == 0);
    assert(split_ai_parse_count("2") == 2);
    assert(split_ai_parse_count("0005") == 5);
    assert(split_ai_parse_count("999999999999999999999999999") == 5);
    assert(split_ai_parse_count("-1") == 0);
    assert(split_ai_parse_count(" 2") == 0);
    assert(split_ai_parse_count("2cars") == 0);
    for (unsigned humans = 2; humans <= 6; ++humans)
        for (unsigned requested = 0; requested <= 5; ++requested){
            unsigned ai = split_ai_cap(requested, humans);
            assert(ai <= requested);
            assert(ai + humans <= 6);
        }
    /* Opponents selector wraps like the native split-screen mod. */
    for (unsigned humans = 2; humans <= 5; ++humans){
        unsigned max = 6 - humans;
        assert(split_ai_step(0, humans, -1) == max);
        assert(split_ai_step(max, humans, 1) == 0);
        for (unsigned n = 0; n <= max; ++n){
            assert(split_ai_step(n, humans, 0) == n);
            assert(split_ai_step(n, humans, -1) == (n ? n-1 : max));
            assert(split_ai_step(n, humans, 1) == (n < max ? n+1 : 0));
        }
    }
    assert(split_ai_step(0, 6, 1) == 0);
    for (unsigned type = 0; type <= 12; ++type){
        int expected = type == 0 || type == 1 || type == 2 || type == 4 ||
                       type == 5 || type == 7 || type == 10;
        assert(split_ai_supported_mode(type) == expected);
    }
    return 0;
}

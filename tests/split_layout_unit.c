/* Standalone layout parity checks for the browser split-screen port. */
#include <assert.h>
#include "../runtime/splitscreen_layout.h"
int main(void){
    SplitCell a = split_layout_cell(2, 0, 1920, 1080, 0);
    SplitCell b = split_layout_cell(2, 1, 1920, 1080, 0);
    assert(a.x == 0 && a.y == 0 && a.w == 1920 && a.h == 540);
    assert(b.x == 0 && b.y == 540 && b.w == 1920 && b.h == 540);
    a = split_layout_cell(2, 0, 1920, 1080, 1);
    b = split_layout_cell(2, 1, 1920, 1080, 1);
    assert(a.x == 0 && a.w == 960 && b.x == 960 && b.w == 960);
    assert(split_layout_cell(6, 5, 1920, 1080, 0).x == 1280);
    assert(split_layout_cell(0, 0, 1920, 1080, 0).w == 0);
    /* Odd-sized canvases must not leave an unrendered right/bottom strip. */
    a = split_layout_cell(2, 0, 1919, 1079, 1);
    b = split_layout_cell(2, 1, 1919, 1079, 1);
    assert(a.w + b.w == 1919 && b.x + b.w == 1919);
    a = split_layout_cell(2, 0, 1919, 1079, 0);
    b = split_layout_cell(2, 1, 1919, 1079, 0);
    assert(a.h + b.h == 1079 && b.y + b.h == 1079);
    for (int n = 4; n <= 6; n += 2){
        SplitCell last = split_layout_cell(n, n-1, 1919, 1079, 0);
        assert(last.x + last.w == 1919);
        assert(last.y + last.h == 1079);
    }
    return 0;
}

/* The console builds' C rules (consoles/common/tetris_core.c), checked on
 * the host against the same expectations as the Lua game's tests. */
#include "tetris_core.h"

#include <stdio.h>
#include <string.h>

static int failures, count;
#define CHECK(c, what) do { ++count; if (!(c)) { ++failures; printf("FAIL: %s\n", what); } } while (0)

/* The falling piece's cells as sorted row*10+col values, "a,b,c,d". */
static void cells(const TcBoard* b, char* out) {
    int v[4], i, j, t;
    for (i = 0; i < 4; ++i)
        v[i] = (b->y + tc_cell_y(b->piece, b->rot, (unsigned char)i)) * 10 + b->x + tc_cell_x(b->piece, b->rot, (unsigned char)i);
    for (i = 0; i < 4; ++i)
        for (j = i + 1; j < 4; ++j)
            if (v[j] < v[i]) { t = v[i]; v[i] = v[j]; v[j] = t; }
    sprintf(out, "%d,%d,%d,%d", v[0], v[1], v[2], v[3]);
}

static void with_piece(TcBoard* b, unsigned char type) {
    tc_new(b, 1, 0, 1234);
    b->next = type;
    /* respawn with the chosen piece */
    b->piece = type; b->rot = 0; b->y = 0;
    b->x = (signed char)((type == 3 || type == 5) ? 5 : 4);
}

int main(void) {
    TcBoard b;
    char s[32];
    const char* spawn[7] = { "3,4,5,6", "3,4,14,15", "5,6,14,15", "4,5,14,15", "4,5,6,16", "3,4,5,13", "3,4,5,14" };
    unsigned char t, r, i, x;
    int frames;

    /* Spawn cells match the original (rows 0-1, columns 3-6). */
    for (t = 1; t <= 7; ++t) {
        tc_new(&b, 1, 0, (unsigned short)(t * 77 + 1));
        while (b.piece != t) tc_new(&b, 1, 0, (unsigned short)(b.rng + 1));
        cells(&b, s);
        CHECK(strcmp(s, spawn[t - 1]) == 0, "spawn cells");
    }

    /* Four turns come back to the start; the O doesn't turn. */
    for (t = 1; t <= 7; ++t)
        for (i = 0; i < 4; ++i) {
            signed char x0 = tc_cell_x(t, 0, i), y0 = tc_cell_y(t, 0, i);
            CHECK(tc_cell_x(t, 4 % 4, i) == x0 && tc_cell_y(t, 0, i) == y0, "rotation 0");
            CHECK(tc_cell_x(t, 1, i) == (t == 4 ? x0 : -y0) && tc_cell_y(t, 1, i) == (t == 4 ? y0 : x0), "clockwise turn");
        }

    /* Walls stop movement; slide goes all the way. */
    with_piece(&b, 4);
    tc_slide(&b, -1);
    CHECK(b.x == 0, "slide stops at the left wall");
    tc_slide(&b, 1);
    CHECK(b.x == 8, "slide stops at the right wall");
    CHECK(!tc_right(&b), "can't go through the wall");

    /* Gravity: level 1 is 600 ms a row, 36 frames at 60 Hz. */
    with_piece(&b, 4);
    for (frames = 0; frames < 35; ++frames) tc_update(&b, 1000);
    CHECK(b.y == 0, "no fall before 600 ms");
    tc_update(&b, 1000);
    CHECK(b.y == 1, "one row at 600 ms");
    b.level = 10;
    b.fall = 0;
    for (frames = 0; frames < 3; ++frames) tc_update(&b, 1000);
    CHECK(b.y == 2, "level 10: a row every 50 ms (3 frames)");

    /* Landing scores 6 x level; a filled row blinks, then goes for 100. */
    tc_new(&b, 3, 0, 99);
    for (x = 0; x < TC_W; ++x) b.grid[TC_H - 1][x] = 1;
    b.grid[TC_H - 1][4] = 0;
    b.grid[TC_H - 1][5] = 0;
    b.piece = 4; b.rot = 0; b.x = 4; b.y = 0;
    b.events = 0;
    tc_drop(&b);
    CHECK(b.score == 18, "6 x level for the landing");
    CHECK(b.events & TC_EV_PLACE, "place event");
    CHECK((b.events & TC_EV_CLEAR) && b.nclear == 1 && b.clearing[0] == TC_H - 1, "one full row waits");
    CHECK(tc_busy(&b), "busy while the row blinks");
    tc_collapse(&b);
    CHECK(b.score == 118 && b.lines == 1, "100 for one row");
    CHECK(b.grid[TC_H - 1][4] == 4 && b.grid[TC_H - 1][0] == 0, "rows above drop");

    /* Four rows at once: 800; level up at the original's line counts. */
    tc_new(&b, 1, 0, 5);
    b.lines = 8;
    b.nclear = 4;
    for (i = 0; i < 4; ++i) b.clearing[i] = (unsigned char)(16 + i);
    b.events = 0;
    tc_collapse(&b);
    CHECK(b.score == 800 && b.lines == 12, "800 for four rows");
    CHECK(b.level == 2 && (b.events & TC_EV_LEVEL), "level 2 at 10 lines");
    b.lines = 91; b.nclear = 1; b.clearing[0] = 19;
    tc_collapse(&b);
    CHECK(b.level == 10, "level 10 at 92 lines");

    /* Starting rows: partly filled, never full or empty. */
    tc_new(&b, 1, 9, 4321);
    for (r = TC_H - 9; r < TC_H; ++r) {
        unsigned char n = 0;
        for (x = 0; x < TC_W; ++x) n = (unsigned char)(n + (b.grid[r][x] != 0));
        CHECK(n > 0 && n < TC_W, "starting row partly filled");
    }
    CHECK(b.grid[TC_H - 10][0] == 0, "rows above are empty");

    /* Game over when a new piece doesn't fit. */
    tc_new(&b, 1, 0, 7);
    for (r = 0; r < 2; ++r)
        for (x = 0; x < TC_W - 1; ++x) b.grid[r][x] = 1;
    b.events = 0;
    b.next = 1;
    b.piece = 4; b.x = 4; b.y = 2;
    tc_drop(&b);
    CHECK(b.over && (b.events & TC_EV_OVER), "game over");

    /* The attract player clears lines over a few hundred pieces, as the Lua
     * tests' autoplayer does. */
    tc_new(&b, 1, 0, 2024);
    while (!b.over && b.placed < 300) {
        unsigned char rot;
        signed char ax;
        if (b.nclear) { tc_collapse(&b); continue; }
        tc_auto_choose(&b, &rot, &ax);
        while (!tc_auto_step(&b, rot, ax)) {}
    }
    printf("autoplay: %u pieces, %u lines, score %lu\n", b.placed, b.lines, b.score);
    CHECK(b.placed >= 300 && b.lines >= 80, "autoplay keeps going and clears lines");

    printf("console core: %d checks, %d failed\n", count, failures);
    return failures ? 1 : 0;
}

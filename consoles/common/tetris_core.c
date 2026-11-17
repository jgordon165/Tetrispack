/* See tetris_core.h. Mirrors scripts/game/board.lua line for line where it
 * can; tests/test_console_core.c checks the two agree on the rules. */
#include "tetris_core.h"

const unsigned short tc_gravity_ms[10] = { 600, 600, 520, 450, 381, 306, 230, 155, 84, 50 };
const unsigned char tc_level_lines[9] = { 10, 16, 24, 34, 36, 50, 64, 78, 92 };
const unsigned short tc_line_points[4] = { 100, 200, 400, 800 };

/* Spawn cells (x, y) about the pivot, and the pivot's spawn column, for
 * I Z S O J L T (types 1-7). */
static const signed char spawn_cells[7][4][2] = {
    { { -1, 0 }, { 0, 0 }, { 1, 0 }, { 2, 0 } },    /* I */
    { { -1, 0 }, { 0, 0 }, { 0, 1 }, { 1, 1 } },    /* Z */
    { { 0, 0 }, { 1, 0 }, { -1, 1 }, { 0, 1 } },    /* S */
    { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } },     /* O */
    { { -1, 0 }, { 0, 0 }, { 1, 0 }, { 1, 1 } },    /* J */
    { { -1, 0 }, { 0, 0 }, { 1, 0 }, { -1, 1 } },   /* L */
    { { -1, 0 }, { 0, 0 }, { 1, 0 }, { 0, 1 } }     /* T */
};
static const signed char spawn_x[7] = { 4, 4, 5, 4, 5, 4, 4 };

/* Clockwise turns with y down: (x, y) -> (-y, x). The O doesn't turn. */
signed char tc_cell_x(unsigned char type, unsigned char rot, unsigned char i) {
    signed char x = spawn_cells[type - 1][i][0], y = spawn_cells[type - 1][i][1], t;
    if (type == 4) return x;
    while (rot--) { t = x; x = (signed char)-y; y = t; }
    return x;
}

signed char tc_cell_y(unsigned char type, unsigned char rot, unsigned char i) {
    signed char x = spawn_cells[type - 1][i][0], y = spawn_cells[type - 1][i][1], t;
    if (type == 4) return y;
    while (rot--) { t = x; x = (signed char)-y; y = t; }
    return y;
}

static unsigned char rotations(unsigned char type) { return type == 4 ? 1 : 4; }

/* xorshift16; returns 1..n. */
static unsigned char rnd(TcBoard* b, unsigned char n) {
    unsigned short r = b->rng;
    r ^= (unsigned short)(r << 7);
    r ^= (unsigned short)(r >> 9);
    r ^= (unsigned short)(r << 8);
    b->rng = r;
    return (unsigned char)(r % n + 1);
}

static unsigned char fits(const TcBoard* b, unsigned char type, unsigned char rot, signed char px, signed char py) {
    unsigned char i;
    signed char x, y;
    for (i = 0; i < 4; ++i) {
        x = (signed char)(px + tc_cell_x(type, rot, i));
        y = (signed char)(py + tc_cell_y(type, rot, i));
        if (x < 0 || x >= TC_W || y >= TC_H) return 0;
        if (y >= 0 && b->grid[y][x]) return 0;
    }
    return 1;
}

static void spawn(TcBoard* b) {
    b->piece = b->next;
    b->next = rnd(b, 7);
    b->rot = 0;
    b->x = spawn_x[b->piece - 1];
    b->y = 0;
    b->fall = 0;
    if (!fits(b, b->piece, b->rot, b->x, b->y)) {
        b->over = 1;
        b->events |= TC_EV_OVER;
    }
}

void tc_new(TcBoard* b, unsigned char level, unsigned char rows, unsigned short seed) {
    unsigned char x, y, count;
    for (y = 0; y < TC_H; ++y)
        for (x = 0; x < TC_W; ++x) b->grid[y][x] = 0;
    b->level = level < 1 ? 1 : level > 10 ? 10 : level;
    b->score = 0;
    b->lines = 0;
    b->placed = 0;
    b->over = 0;
    b->nclear = 0;
    b->fall = 0;
    b->events = 0;
    b->rng = seed ? seed : 0xACE1;
    if (rows > 12) rows = 12;
    /* Skill > Starting Rows: each cell filled at random, at least one gap
     * and one block per row (as the Lua game; the original's pattern isn't
     * confirmed). */
    for (y = (unsigned char)(TC_H - rows); y < TC_H; ++y) {
        do {
            count = 0;
            for (x = 0; x < TC_W; ++x) {
                if (rnd(b, 2) == 1) { b->grid[y][x] = rnd(b, 7); ++count; }
                else b->grid[y][x] = 0;
            }
        } while (count == 0 || count == TC_W);
    }
    b->next = rnd(b, 7);
    spawn(b);
}

unsigned char tc_busy(const TcBoard* b) { return b->over || b->nclear; }

static unsigned char try_move(TcBoard* b, signed char dx, signed char dy, unsigned char drot) {
    unsigned char rot;
    if (tc_busy(b)) return 0;
    rot = (unsigned char)((b->rot + drot) % rotations(b->piece));
    if (!fits(b, b->piece, rot, (signed char)(b->x + dx), (signed char)(b->y + dy))) return 0;
    b->rot = rot;
    b->x = (signed char)(b->x + dx);
    b->y = (signed char)(b->y + dy);
    return 1;
}

unsigned char tc_left(TcBoard* b) { return try_move(b, -1, 0, 0); }
unsigned char tc_right(TcBoard* b) { return try_move(b, 1, 0, 0); }

unsigned char tc_rotate(TcBoard* b) {
    if (!try_move(b, 0, 0, 1)) return 0;
    b->events |= TC_EV_ROTATE;
    return 1;
}

void tc_slide(TcBoard* b, signed char dx) { while (try_move(b, dx, 0, 0)) {} }

/* Lands the piece; full rows wait in `clearing` for tc_collapse. */
static void lock(TcBoard* b) {
    unsigned char i, x, y, full;
    signed char cy;
    for (i = 0; i < 4; ++i) {
        cy = (signed char)(b->y + tc_cell_y(b->piece, b->rot, i));
        if (cy < 0) { b->over = 1; b->events |= TC_EV_OVER; return; }
        x = (unsigned char)(b->x + tc_cell_x(b->piece, b->rot, i));
        y = (unsigned char)cy;
        b->grid[y][x] = b->piece;
    }
    ++b->placed;
    b->score += (tc_score)6 * b->level;
    b->events |= TC_EV_PLACE;
    b->nclear = 0;
    for (y = 0; y < TC_H; ++y) {
        full = 1;
        for (x = 0; x < TC_W; ++x)
            if (!b->grid[y][x]) { full = 0; break; }
        if (full && b->nclear < 4) b->clearing[b->nclear++] = y;
    }
    if (b->nclear) { b->events |= TC_EV_CLEAR; return; }
    spawn(b);
}

void tc_drop(TcBoard* b) {
    if (tc_busy(b)) return;
    while (try_move(b, 0, 1, 0)) {}
    lock(b);
}

void tc_update(TcBoard* b, unsigned short ms60) {
    unsigned short delay;
    if (tc_busy(b)) return;
    /* The delay in 1/60 ms: at most 600 * 60 = 36000, fits 16 bits. */
    delay = (unsigned short)(tc_gravity_ms[b->level - 1] * 60U);
    b->fall = (unsigned short)(b->fall + ms60);
    while (b->fall >= delay && !tc_busy(b)) {
        b->fall = (unsigned short)(b->fall - delay);
        if (!try_move(b, 0, 1, 0)) lock(b);
    }
}

void tc_collapse(TcBoard* b) {
    unsigned char x, i, n = b->nclear, gone;
    signed char y, dst;
    if (!n) return;
    dst = TC_H - 1;
    for (y = TC_H - 1; y >= 0; --y) {
        gone = 0;
        for (i = 0; i < n; ++i) if (b->clearing[i] == (unsigned char)y) gone = 1;
        if (gone) continue;
        if (dst != y)
            for (x = 0; x < TC_W; ++x) b->grid[dst][x] = b->grid[y][x];
        --dst;
    }
    for (; dst >= 0; --dst)
        for (x = 0; x < TC_W; ++x) b->grid[dst][x] = 0;
    b->nclear = 0;
    b->score += tc_line_points[n - 1];
    b->lines = (unsigned short)(b->lines + n);
    for (i = 0; i < 9; ++i)
        if (b->lines >= tc_level_lines[i] && b->level < i + 2) {
            b->level = (unsigned char)(i + 2);
            b->events |= TC_EV_LEVEL;
        }
    spawn(b);
}

/* ---- Attract mode ---------------------------------------------------------- */

/* The board without the falling piece, worked out once per choice: each
 * column's height and holes. A candidate then only rechecks the (at most
 * four) columns and rows its cells touch, which keeps the search quick on
 * the SNES. */
typedef struct {
    unsigned char height[TC_W], holes[TC_W];
} Profile;

static void profile(const TcBoard* b, Profile* p) {
    unsigned char x, y, top;
    for (x = 0; x < TC_W; ++x) {
        top = TC_H;
        p->holes[x] = 0;
        for (y = 0; y < TC_H; ++y) {
            if (b->grid[y][x]) { if (top == TC_H) top = y; }
            else if (top < TC_H) ++p->holes[x];
        }
        p->height[x] = (unsigned char)(TC_H - top);
    }
}

/* The board with the piece landed at cells (cx, cy), scored as
 * autoplay.lua does (weights x10, so it fits 16 bits: the SNES compiler's
 * long arithmetic is 16-bit): lines 3.4, holes -3.6, height -0.5,
 * bumpiness -0.2. */
static short evaluate(const TcBoard* b, const Profile* base, const unsigned char* cx, const unsigned char* cy) {
    unsigned char height[TC_W];
    unsigned char i, k, x, y, top, full, mine, done;
    short lines = 0, holes = 0, sum = 0, bump = 0;
    for (x = 0; x < TC_W; ++x) { height[x] = base->height[x]; holes += base->holes[x]; }
    /* The columns the piece is in: their height and holes again. */
    for (i = 0; i < 4; ++i) {
        x = cx[i];
        done = 0;
        for (k = 0; k < i; ++k) if (cx[k] == x) done = 1;
        if (done) continue;
        holes -= base->holes[x];
        top = TC_H;
        for (y = 0; y < TC_H; ++y) {
            mine = 0;
            for (k = 0; k < 4; ++k) if (cx[k] == x && cy[k] == y) mine = 1;
            if (b->grid[y][x] || mine) { if (top == TC_H) top = y; }
            else if (top < TC_H) ++holes;
        }
        height[x] = (unsigned char)(TC_H - top);
    }
    /* Rows the piece fills up. */
    for (i = 0; i < 4; ++i) {
        y = cy[i];
        done = 0;
        for (k = 0; k < i; ++k) if (cy[k] == y) done = 1;
        if (done) continue;
        full = 1;
        for (x = 0; x < TC_W && full; ++x) {
            mine = 0;
            for (k = 0; k < 4; ++k) if (cx[k] == x && cy[k] == y) mine = 1;
            if (!b->grid[y][x] && !mine) full = 0;
        }
        lines += full;
    }
    for (x = 0; x < TC_W; ++x) {
        sum += height[x];
        if (x) bump += height[x] > height[x - 1] ? height[x] - height[x - 1] : height[x - 1] - height[x];
    }
    return (short)(lines * 34 - holes * 36 - sum * 5 - bump * 2);
}

void tc_auto_choose(const TcBoard* b, unsigned char* best_rot, signed char* best_x) {
    unsigned char rot, i, n, ok, cx[4], cy[4];
    signed char x, y, land, rx[4][4], ry[4][4], c;
    short score, best = -32767;
    Profile base;
    profile(b, &base);
    *best_rot = b->rot;
    *best_x = b->x;
    n = rotations(b->piece);
    for (rot = 0; rot < n; ++rot)
        for (i = 0; i < 4; ++i) {
            rx[rot][i] = tc_cell_x(b->piece, rot, i);
            ry[rot][i] = tc_cell_y(b->piece, rot, i);
        }
    for (rot = 0; rot < n; ++rot)
        for (x = -3; x <= TC_W + 2; ++x) {
            /* Dropped from the top, the piece stops on each column's
             * highest block: it lands at the lowest y every cell allows. */
            ok = 1;
            land = TC_H;
            for (i = 0; i < 4 && ok; ++i) {
                c = (signed char)(x + rx[rot][i]);
                if (c < 0 || c >= TC_W) { ok = 0; break; }
                y = (signed char)(TC_H - base.height[(unsigned char)c] - 1 - ry[rot][i]);
                if (y < land) land = y;
            }
            if (!ok || land < b->y) continue;
            for (i = 0; i < 4; ++i) {
                if (land + ry[rot][i] < 0) ok = 0;   /* would stick out of the top */
                cx[i] = (unsigned char)(x + rx[rot][i]);
                cy[i] = (unsigned char)(land + ry[rot][i]);
            }
            if (!ok) continue;
            score = evaluate(b, &base, cx, cy);
            if (score > best) { best = score; *best_rot = rot; *best_x = x; }
        }
}

unsigned char tc_auto_step(TcBoard* b, unsigned char rot, signed char x) {
    if (tc_busy(b)) return 0;
    if (b->rot != rot) { if (!tc_rotate(b)) { tc_drop(b); return 1; } }
    else if (b->x < x) { if (!tc_right(b)) { tc_drop(b); return 1; } }
    else if (b->x > x) { if (!tc_left(b)) { tc_drop(b); return 1; } }
    else { tc_drop(b); return 1; }
    return 0;
}

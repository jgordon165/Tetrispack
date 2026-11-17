/* Tetris for Windows 2.00's rules in plain C for the console builds (SGDK on
 * the Mega Drive / Genesis, PVSnesLib on the SNES), where the Lua game can't
 * run. The same rules as scripts/game/board.lua and pieces.lua: the
 * original's pieces, spawn cells, gravity timer, level-up line counts and
 * scoring (docs/original.md). Integer-only C89 so both consoles' compilers
 * take it; no drawing, input or sound (the caller reads `events`).
 */
#ifndef TETRIS_CORE_H
#define TETRIS_CORE_H

#define TC_W 10
#define TC_H 20

/* Bits in TcBoard.events, set by the calls below; the caller clears them. */
#define TC_EV_ROTATE 0x01   /* a rotation went through */
#define TC_EV_PLACE  0x02   /* a piece landed */
#define TC_EV_CLEAR  0x04   /* rows filled (nclear of them; blink, then tc_collapse) */
#define TC_EV_LEVEL  0x08   /* the level went up */
#define TC_EV_OVER   0x10   /* game over */

typedef struct {
    unsigned char grid[TC_H][TC_W];   /* 0 empty, 1-7 the piece type (= colour) */
    unsigned char piece, rot;         /* falling piece type 1-7, rotation 0-3 */
    signed char x, y;                 /* its pivot cell */
    unsigned char next;               /* the next piece's type */
    unsigned char level;              /* 1-10 */
    unsigned long score;
    unsigned short lines;
    unsigned short placed;            /* pieces landed so far */
    unsigned char over;
    unsigned char clearing[4];        /* full rows waiting for tc_collapse (top to bottom) */
    unsigned char nclear;
    unsigned short fall;              /* gravity timer, in 1/60 ms */
    unsigned short rng;
    unsigned char events;
} TcBoard;

/* Cell `i` (0-3) of piece `type` (1-7) in rotation `rot`. */
signed char tc_cell_x(unsigned char type, unsigned char rot, unsigned char i);
signed char tc_cell_y(unsigned char type, unsigned char rot, unsigned char i);

/* A new game at `level` (1-10) with `rows` (0-12) partly filled rows. `seed`
 * picks the pieces (any non-zero value). */
void tc_new(TcBoard* b, unsigned char level, unsigned char rows, unsigned short seed);

/* True while a game can't take moves: over, or rows waiting to go. */
unsigned char tc_busy(const TcBoard* b);

unsigned char tc_left(TcBoard* b);
unsigned char tc_right(TcBoard* b);
unsigned char tc_rotate(TcBoard* b);
void tc_slide(TcBoard* b, signed char dx);   /* End / Page Down: all the way */
void tc_drop(TcBoard* b);                    /* Down: all the way, then land */

/* Advances gravity by one video frame: `ms60` is the frame's length in
 * 1/60 ms (1000 at 60 Hz, 1200 at 50 Hz). */
void tc_update(TcBoard* b, unsigned short ms60);

/* Removes the rows in `clearing`, scores them and brings in the next piece. */
void tc_collapse(TcBoard* b);

/* The attract mode's player (scripts/game/autoplay.lua): picks the best
 * rotation and column for the falling piece, then tc_auto_step makes one
 * move toward them per call (turn, then slide, then drop). Returns 1 once
 * the piece has been dropped. */
void tc_auto_choose(const TcBoard* b, unsigned char* rot, signed char* x);
unsigned char tc_auto_step(TcBoard* b, unsigned char rot, signed char x);

/* The original's tables. */
extern const unsigned short tc_gravity_ms[10];
extern const unsigned char tc_level_lines[9];
extern const unsigned short tc_line_points[4];

#endif

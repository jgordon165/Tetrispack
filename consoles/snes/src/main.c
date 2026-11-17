/* Tetrispack for the Super Nintendo (PVSnesLib).
 *
 * Tetris for Windows 2.00 on a 256x224 screen: the level background tiled
 * behind everything, the score panel on the left, the playfield in its gray
 * 3-D frame, full rows blinking (inverted) before they go. The rules are
 * consoles/common/tetris_core.c (shared with the Genesis build); the art is
 * tools/console_art.py's (art.inc); the music is the PC game's SNES style as
 * Impulse Tracker modules for SNESMOD, the sound effects the PC game's.
 *
 * Mode 1. BG2 holds the background, frames and panels; BG1 the blocks and
 * text. Palettes: 0 background, 1 blocks, 2 frames and text, 3 inverted
 * blocks (the blink). Both tilemaps live in RAM and go to VRAM in vblank
 * when they change.
 *
 * Pad: Left / Right move, Up / A / X rotate, Down / B drop, L / R slide to
 * the wall, Start pauses. On the title screen Up / Down pick a setting,
 * Left / Right change it, Start plays; left alone for 10 seconds it plays a
 * demo by itself (any button goes back).
 */
#include <snes.h>

#include "art.h"
#include "res/soundbank.h"
#include "soundbanks.h"
#include "tetris_core.h"

#define WELL_X 11
#define WELL_Y 4
#define PANEL_X 1
#define PANEL_Y 4
#define PANEL_W 8
#define PANEL_H 18
#define NEXT_X 2
#define NEXT_Y 14

#define BLINK_PASSES 10
#define BLINK_FRAMES 4
#define DAS_DELAY 14
#define DAS_RATE 4
#define DEMO_AFTER 600
#define DEMO_MOVE 6

#define VRAM_TILES 0x0000
#define VRAM_BG_TILES (ART_TILES * 16)
#define VRAM_MAP1 0x6000
#define VRAM_MAP2 0x6400

#define BG_BASE ART_TILES
#define T(pal, idx) ((u16)(((pal) << 10) | (idx)))

#define TITLE 0
#define PLAY 1
#define PAUSE 2
#define OVER 3
#define DEMO 4

TcBoard board;
u16 map1[32 * 32], map2[32 * 32];
u8 dirty1, dirty2;
u8 state;
u8 start_level, start_rows, preview, music_on;
u8 menu_item, blink, blink_t, loaded_bg;
s8 song;
u16 shown[TC_H][TC_W];
tc_score shown_score;
u16 shown_lines;
u8 shown_level, shown_next;
u16 pad, pad_prev, held_frames, frame_count, idle;
u8 demo_rot, demo_wait, demo_planned;
signed char demo_x;
u8 bg_pending;

const u8 rows_choices[6] = { 0, 1, 3, 5, 7, 9 };

u8 piece_mask[TC_H * TC_W];
/* What the well showed last time: it's only redrawn when one of these
 * changes (the 65816 can't afford 200 cells every frame). */
u8 drawn_piece, drawn_rot, drawn_state, drawn_blink, drawn_valid;
signed char drawn_x, drawn_y;
u16 drawn_placed, drawn_lines;


/* ---- tilemaps in RAM ---------------------------------------------------- */

void put1(u16 x, u16 y, u16 t) { map1[(y << 5) + x] = t; dirty1 = 1; }
void put2(u16 x, u16 y, u16 t) { map2[(y << 5) + x] = t; dirty2 = 1; }

void fill1(u16 x, u16 y, u16 w, u16 h, u16 t) {
    u16 i, j;
    for (j = 0; j < h; ++j)
        for (i = 0; i < w; ++i) map1[((y + j) << 5) + x + i] = t;
    dirty1 = 1;
}

void fill2(u16 x, u16 y, u16 w, u16 h, u16 t) {
    u16 i, j;
    for (j = 0; j < h; ++j)
        for (i = 0; i < w; ++i) map2[((y + j) << 5) + x + i] = t;
    dirty2 = 1;
}

void text(u16 x, u16 y, char* s, u16 font) {
    while (*s) {
        put1(x, y, T(2, font + (u8)(*s - 32)));
        ++s;
        ++x;
    }
}

u16 length(char* s) {
    u16 n = 0;
    while (s[n]) ++n;
    return n;
}

void text_center(u16 y, char* s, u16 font) { text((32 - length(s)) >> 1, y, s, font); }

void number(char* out, tc_score v) {
    char tmp[12];
    u8 n = 0, i = 0;
    do { tmp[n++] = (char)('0' + (u8)(v % 10)); v = v / 10; } while (v);
    while (n) out[i++] = tmp[--n];
    out[i] = 0;
}

void copy(char* to, char* from) { while ((*to++ = *from++) != 0) {} }

/* A 3x3-tile style box on BG2 (TL T TR L C R BL B BR from `base`). */
void box(u16 x, u16 y, u16 w, u16 h, u16 base, u16 center) {
    u16 i;
    fill2(x + 1, y + 1, w - 2, h - 2, T(2, center));
    for (i = 1; i < w - 1; ++i) {
        put2(x + i, y, T(2, base + 1));
        put2(x + i, y + h - 1, T(2, base + 7));
    }
    for (i = 1; i < h - 1; ++i) {
        put2(x, y + i, T(2, base + 3));
        put2(x + w - 1, y + i, T(2, base + 5));
    }
    put2(x, y, T(2, base));
    put2(x + w - 1, y, T(2, base + 2));
    put2(x, y + h - 1, T(2, base + 6));
    put2(x + w - 1, y + h - 1, T(2, base + 8));
}

/* The level's background (BITMAP n): 4x4 tiles, palette 0; the tiles and
 * colours go up in the next vblank. */
void background(u8 level) {
    u16 x, y;
    if ((u8)(level - 1) == loaded_bg) return;
    loaded_bg = (u8)(level - 1);
    bg_pending = 1;
    for (y = 0; y < 28; ++y)
        for (x = 0; x < 32; ++x)
            map2[(y << 5) + x] = T(0, BG_BASE + ((y & 3) << 2) + (x & 3));
    dirty2 = 1;
}

/* After WaitForVBlank: whatever changed, to VRAM / CGRAM. */
void upload(void) {
    if (bg_pending) {
        dmaCopyVram(&art_bg_tiles + loaded_bg * (ART_BG_TILES * 32), VRAM_BG_TILES, ART_BG_TILES * 32);
        dmaCopyCGram(&art_bg_pal + loaded_bg * 32, 0, 32);
        bg_pending = 0;
    }
    if (dirty2) { dmaCopyVram((u8*)map2, VRAM_MAP2, 2048); dirty2 = 0; }
    if (dirty1) { dmaCopyVram((u8*)map1, VRAM_MAP1, 2048); dirty1 = 0; }
}

/* ---- sound -------------------------------------------------------------- */

void load_effects(void) {
    u8 j;
    for (j = 0; j < 4; ++j) spcLoadEffect(j);
}

void play_song(s8 s) {
    if (!music_on) s = -1;
    if (s == song) return;
    song = s;
    spcStop();
    if (s >= 0) {
        spcLoad(MOD_SONG1 + s);
        load_effects();
        spcPlay(0);
    }
}

s8 song_for(u8 level) { return (s8)((level - 1) / 2 > 4 ? 4 : (level - 1) / 2); }

/* 8 kHz samples (pitch 2), full volume, centred. */
void sfx(u8 n) { spcEffect(2, n, 15 * 16 + 8); }

/* ---- title screen ------------------------------------------------------- */

void title_options(void) {
    char v[8];
    u8 i;
    u16 y, font;
    for (i = 0; i < 4; ++i) {
        y = 15 + i;
        font = (i == menu_item) ? ART_FONT_BLUE : ART_FONT;
        fill1(3, y, 26, 1, 0);
        if (i == 0) { text(3, y, "Starting Level", font); number(v, start_level); }
        else if (i == 1) { text(3, y, "Starting Rows", font); number(v, start_rows); }
        else if (i == 2) { text(3, y, "Piece Preview", font); copy(v, preview ? "On" : "Off"); }
        else { text(3, y, "Music", font); copy(v, music_on ? "On" : "Off"); }
        text(20, y, "<", ART_FONT);
        text(22, y, v, font);
        text(26, y, ">", ART_FONT);
    }
}

void title(void) {
    state = TITLE;
    idle = 0;
    fill1(0, 0, 32, 32, 0);
    loaded_bg = 0xFF;
    background(1);
    box(1, 2, 30, 24, ART_ABOUT, ART_ABOUT + 4);
    text_center(4, "TETRISPACK", ART_FONT);
    text_center(6, "Tetris for Windows 2.00", ART_FONT);
    text_center(7, "on the Super Nintendo", ART_FONT);
    text_center(9, "Original Concept by", ART_FONT);
    text_center(10, "Alexey Pajitnov", ART_FONT);
    text_center(11, "Windows version by", ART_FONT);
    text_center(12, "Dave Edson", ART_FONT);
    title_options();
    text_center(22, "Press START", ART_FONT);
    play_song(0);
}

/* ---- the game screen ---------------------------------------------------- */

void layout(void) {
    u8 x, y;
    fill1(0, 0, 32, 32, 0);
    loaded_bg = 0xFF;
    background(board.level);
    box(WELL_X - 1, WELL_Y - 1, TC_W + 2, TC_H + 2, ART_FRAME_BLACK, ART_FRAME_BLACK + 4);
    box(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, ART_FRAME_BLACK, ART_FRAME_GRAY);
    box(NEXT_X, NEXT_Y, 6, 6, ART_SUNKEN, ART_SUNKEN + 4);
    text(PANEL_X + 1, PANEL_Y + 1, "Score:", ART_FONT);
    text(PANEL_X + 1, PANEL_Y + 4, "Level:", ART_FONT);
    text(PANEL_X + 1, PANEL_Y + 7, "Lines:", ART_FONT);
    for (y = 0; y < TC_H; ++y)
        for (x = 0; x < TC_W; ++x) shown[y][x] = 0;
    shown_score = 0xFFFFFFFF;
    shown_lines = 0xFFFF;
    shown_level = 0xFF;
    shown_next = 0xFF;
    drawn_valid = 0;
}

void new_game(u8 demo) {
    tc_new(&board, demo ? 1 : start_level, demo ? 0 : start_rows, (u16)(frame_count * 31 + rand() + 1));
    blink = 0;
    demo_planned = 0;
    demo_wait = 0;
    state = demo ? DEMO : PLAY;
    layout();
    play_song(song_for(board.level));
}

void draw_next(void) {
    u8 want, i;
    signed char minx, maxx, miny, maxy, cx, cy;
    u16 ox, oy;
    want = (preview && !board.over) ? board.next : 0;
    if (want == shown_next) return;
    shown_next = want;
    fill1(NEXT_X + 1, NEXT_Y + 1, 4, 4, 0);
    if (!want) return;
    minx = 9; maxx = -9; miny = 9; maxy = -9;
    for (i = 0; i < 4; ++i) {
        cx = tc_cell_x(want, 0, i);
        cy = tc_cell_y(want, 0, i);
        if (cx < minx) minx = cx;
        if (cx > maxx) maxx = cx;
        if (cy < miny) miny = cy;
        if (cy > maxy) maxy = cy;
    }
    ox = NEXT_X + 1 + ((4 - (maxx - minx + 1)) >> 1);
    oy = NEXT_Y + 1 + ((4 - (maxy - miny + 1)) >> 1);
    fill1(ox, oy, maxx - minx + 1, maxy - miny + 1, T(2, ART_FRAME_BLACK + 4));
    for (i = 0; i < 4; ++i)
        put1(ox + tc_cell_x(want, 0, i) - minx, oy + tc_cell_y(want, 0, i) - miny, T(1, ART_BLOCK + want - 1));
}

void draw_values(void) {
    char s[12];
    if (board.score != shown_score) {
        shown_score = board.score;
        number(s, board.score);
        fill1(PANEL_X + 1, PANEL_Y + 2, 6, 1, 0);
        text(PANEL_X + 1, PANEL_Y + 2, s, ART_FONT_BLUE);
    }
    if (board.level != shown_level) {
        shown_level = board.level;
        number(s, board.level);
        fill1(PANEL_X + 2, PANEL_Y + 5, 3, 1, 0);
        text(PANEL_X + 2, PANEL_Y + 5, s, ART_FONT_BLUE);
        background(board.level);
    }
    if (board.lines != shown_lines) {
        shown_lines = board.lines;
        number(s, board.lines);
        fill1(PANEL_X + 2, PANEL_Y + 8, 4, 1, 0);
        text(PANEL_X + 2, PANEL_Y + 8, s, ART_FONT_BLUE);
    }
    draw_next();
}


void draw_well(void) {
    u8 x, y, i, inv, n, show_piece, *grid, *mask;
    u16 t, *seen, k;
    signed char px[4], py[4];
    if (drawn_valid && drawn_piece == board.piece && drawn_rot == board.rot && drawn_x == board.x &&
        drawn_y == board.y && drawn_state == state && drawn_blink == blink &&
        drawn_placed == board.placed && drawn_lines == board.lines)
        return;
    drawn_valid = 1;
    drawn_piece = board.piece; drawn_rot = board.rot; drawn_x = board.x; drawn_y = board.y;
    drawn_state = state; drawn_blink = blink; drawn_placed = board.placed; drawn_lines = board.lines;

    show_piece = state != PAUSE && !tc_busy(&board);
    if (show_piece)
        for (i = 0; i < 4; ++i) {
            px[i] = (signed char)(board.x + tc_cell_x(board.piece, board.rot, i));
            py[i] = (signed char)(board.y + tc_cell_y(board.piece, board.rot, i));
            if (py[i] >= 0) piece_mask[(u8)py[i] * TC_W + (u8)px[i]] = 1;
        }
    grid = &board.grid[0][0];
    mask = piece_mask;
    seen = &shown[0][0];
    k = (WELL_Y << 5) + WELL_X;
    for (y = 0; y < TC_H; ++y) {
        inv = 0;
        if (blink & 1)
            for (i = 0; i < board.nclear; ++i)
                if (board.clearing[i] == y) inv = 1;
        for (x = 0; x < TC_W; ++x) {
            n = *grid++;
            if (state == PAUSE) t = 0;
            else if (*mask) t = T(1, ART_BLOCK + board.piece - 1);
            else t = n ? T(inv ? 3 : 1, ART_BLOCK + n - 1) : 0;
            ++mask;
            if (t != *seen) {
                *seen = t;
                map1[k + x] = t;
                dirty1 = 1;
            }
            ++seen;
        }
        k += 32;
    }
    if (show_piece)
        for (i = 0; i < 4; ++i)
            if (py[i] >= 0) piece_mask[(u8)py[i] * TC_W + (u8)px[i]] = 0;
    if (state == PAUSE) text(WELL_X + 2, WELL_Y + 9, "Paused", ART_FONT_WHITE);
    if (state == OVER) text(WELL_X, WELL_Y + 9, "Game Over!", ART_FONT_WHITE);
}

/* ---- input -------------------------------------------------------------- */

u16 pressed(u16 b) { return (pad & b) && !(pad_prev & b); }

u8 repeat(u16 b) {
    if (!(pad & b)) return 0;
    if (!(pad_prev & b)) return 1;
    return held_frames >= DAS_DELAY && ((held_frames - DAS_DELAY) % DAS_RATE) == 0;
}

void title_input(void) {
    s8 d;
    u8 i;
    if (pressed(KEY_UP)) { menu_item = (u8)((menu_item + 3) & 3); title_options(); }
    if (pressed(KEY_DOWN)) { menu_item = (u8)((menu_item + 1) & 3); title_options(); }
    if (pressed(KEY_LEFT) || pressed(KEY_RIGHT)) {
        d = pressed(KEY_RIGHT) ? 1 : -1;
        if (menu_item == 0) start_level = (u8)((start_level - 1 + 10 + d) % 10 + 1);
        else if (menu_item == 1) {
            for (i = 0; i < 6 && rows_choices[i] != start_rows; ++i) {}
            start_rows = rows_choices[(i + 6 + d) % 6];
        } else if (menu_item == 2) preview = !preview;
        else { music_on = !music_on; song = -2; play_song(0); }
        title_options();
    }
    if (pressed(KEY_START) || pressed(KEY_A)) new_game(0);
    else if (pad) idle = 0;
    else if (++idle >= DEMO_AFTER) new_game(1);
}

void demo_input(void) {
    if (pad && !pad_prev) { title(); return; }
    if (tc_busy(&board)) return;
    if (!demo_planned) {
        tc_auto_choose(&board, &demo_rot, &demo_x);
        demo_planned = 1;
    }
    if (++demo_wait < DEMO_MOVE) return;
    demo_wait = 0;
    if (tc_auto_step(&board, demo_rot, demo_x)) demo_planned = 0;
}

void play_input(void) {
    if (pressed(KEY_START)) {
        state = PAUSE;
        spcPauseMusic();
        return;
    }
    if (tc_busy(&board)) return;
    if (repeat(KEY_LEFT)) tc_left(&board);
    if (repeat(KEY_RIGHT)) tc_right(&board);
    if (pressed(KEY_UP) || pressed(KEY_A) || pressed(KEY_X)) tc_rotate(&board);
    if (pressed(KEY_DOWN) || pressed(KEY_B)) tc_drop(&board);
    if (pressed(KEY_L)) tc_slide(&board, -1);
    if (pressed(KEY_R)) tc_slide(&board, 1);
}

void events(void) {
    u8 e = board.events;
    board.events = 0;
    if (e & TC_EV_ROTATE) sfx(0);
    if (e & TC_EV_PLACE) sfx(1);
    if (e & TC_EV_CLEAR) {
        sfx(board.nclear >= 3 ? 3 : 2);
        blink = BLINK_PASSES;
        blink_t = 0;
    }
    if (e & TC_EV_LEVEL) play_song(song_for(board.level));
    if (e & TC_EV_OVER) {
        if (state == DEMO) { title(); return; }
        state = OVER;
        play_song(-1);
    }
}

int main(void) {
    start_level = 1;
    preview = 1;
    music_on = 1;
    song = -1;
    loaded_bg = 0xFF;

    spcBoot();
    SET_SOUNDBANKS();
    spcStop();
    spcLoad(MOD_SFX);
    load_effects();

    setMode(BG_MODE1, 0);
    bgSetGfxPtr(0, VRAM_TILES);
    bgSetGfxPtr(1, VRAM_TILES);
    bgSetMapPtr(0, VRAM_MAP1, SC_32x32);
    bgSetMapPtr(1, VRAM_MAP2, SC_32x32);
    bgSetDisable(2);
    dmaCopyVram(&art_tiles, VRAM_TILES, ART_TILES * 32);
    dmaCopyCGram(&art_pals, 16, 96);   /* palettes 1-3 */
    title();
    upload();
    setScreenOn();

    while (1) {
        ++frame_count;
        pad_prev = pad;
        pad = padsCurrent(0);
        if ((pad & (KEY_LEFT | KEY_RIGHT)) && (pad & (KEY_LEFT | KEY_RIGHT)) == (pad_prev & (KEY_LEFT | KEY_RIGHT)))
            ++held_frames;
        else
            held_frames = 0;

        if (state == TITLE) {
            title_input();
        } else if (state == PLAY || state == DEMO) {
            if (state == DEMO) demo_input(); else play_input();
            if (state == PLAY || state == DEMO) {
                if (blink) {
                    if (++blink_t >= BLINK_FRAMES) {
                        blink_t = 0;
                        if (--blink == 0) tc_collapse(&board);
                    }
                } else {
                    tc_update(&board, snes_50hz ? 1200 : 1000);
                }
                events();
            }
        } else if (state == PAUSE) {
            if (pressed(KEY_START)) {
                fill1(WELL_X, WELL_Y + 9, TC_W, 1, 0);
                drawn_valid = 0;
                state = PLAY;
                spcResumeMusic();
            }
        } else if (state == OVER) {
            if (pressed(KEY_START)) title();
        }
        if (state != TITLE) {
            draw_well();
            draw_values();
        }
        spcProcess();
        WaitForVBlank();
        upload();
    }
    return 0;
}

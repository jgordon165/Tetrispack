/* Tetrispack for the Sega Mega Drive / Genesis (SGDK).
 *
 * Tetris for Windows 2.00 on a 320x224 screen: the level background tiled
 * behind everything, the score panel on the left, the playfield in its gray
 * 3-D frame, full rows blinking (inverted) before they go. The rules are
 * consoles/common/tetris_core.c; the art comes from tools/console_art.py
 * (art.c), the music and sound effects from the PC game's Genesis music
 * style (tools/render_music.cpp -> VGM -> XGM2, res/resources.res).
 *
 * Planes: B holds the background, frames and panels; A holds the blocks and
 * text. Palettes: 0 background, 1 blocks, 2 frames and text, 3 inverted
 * blocks (the blink).
 *
 * Pad: Left / Right move, Up / A / C rotate, Down / B drop, X / Z slide to
 * the wall (six-button pads), Start pauses. On the title screen Up / Down
 * pick a setting, Left / Right change it, Start plays. Left alone on the
 * title screen for 10 seconds, it plays a demo game by itself (any button
 * goes back).
 */
#include <genesis.h>

#include "art.h"
#include "resources.h"
#include "tetris_core.h"

/* Screen layout in tiles. */
#define WELL_X 15             /* the playfield's black: 10 x 20 cells */
#define WELL_Y 4
#define PANEL_X 1             /* score panel frame (outer) */
#define PANEL_Y 4
#define PANEL_W 10
#define PANEL_H 17
#define NEXT_X 3              /* next-piece box (outer, 6 x 6) */
#define NEXT_Y 14

#define BLINK_PASSES 10       /* the original inverts full rows 10 times */
#define BLINK_FRAMES 4        /* about its 60 ms a pass */
#define DAS_DELAY 14          /* held Left / Right: first repeat after this many frames */
#define DAS_RATE 4            /* then every this many */
#define DEMO_AFTER 600        /* title screen idle frames before the demo */
#define DEMO_MOVE 6           /* the demo player moves every this many frames */

#define TILE(pal, idx) TILE_ATTR_FULL(pal, 0, 0, 0, TILE_USER_INDEX + (idx))
#define BG_BASE (ART_TILES)

enum { TITLE, PLAY, PAUSE, OVER, DEMO };

static TcBoard board;
static u8 state;
static u8 start_level = 1, start_rows = 0, preview = 1, music_on = 1;
static u8 menu_item;
static u8 blink, blink_t;
static u8 loaded_bg = 0xFF;
static s8 track = -1;
static u16 shown[TC_H][TC_W];   /* plane A tiles now in the well */
static u32 shown_score = 0xFFFFFFFF;
static u16 shown_lines = 0xFFFF;
static u8 shown_level = 0xFF, shown_next = 0xFF;
static u16 pad, pad_prev, held_frames;
static u16 frame_count, idle;
static u8 demo_rot, demo_wait;
static signed char demo_x;   /* the core's type (SGDK's s8 is plain char) */
static u8 demo_planned;

static const u8 rows_choices[6] = { 0, 1, 3, 5, 7, 9 };

/* ---- drawing helpers ------------------------------------------------------ */

static void text(u16 x, u16 y, const char* s, u16 font) {
    for (; *s; ++s, ++x) VDP_setTileMapXY(BG_A, TILE(PAL2, font + (u8)(*s - 32)), x, y);
}

static void text_center(u16 y, const char* s, u16 font) {
    u16 n = strlen(s);
    text((40 - n) / 2, y, s, font);
}

static void clear_a(u16 x, u16 y, u16 w, u16 h) { VDP_fillTileMapRect(BG_A, 0, x, y, w, h); }

static void number(char* out, u32 v) {
    char tmp[12];
    u8 n = 0, i = 0;
    do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) out[i++] = tmp[--n];
    out[i] = 0;
}

/* A 3x3-tile style box: corners and edges from `base` (TL T TR L C R BL B
 * BR), `center` for the inside; outer size w x h tiles. */
static void box(u16 x, u16 y, u16 w, u16 h, u16 base, u16 center) {
    u16 i;
    VDP_fillTileMapRect(BG_B, TILE(PAL2, center), x + 1, y + 1, w - 2, h - 2);
    for (i = 1; i < w - 1; ++i) {
        VDP_setTileMapXY(BG_B, TILE(PAL2, base + 1), x + i, y);
        VDP_setTileMapXY(BG_B, TILE(PAL2, base + 7), x + i, y + h - 1);
    }
    for (i = 1; i < h - 1; ++i) {
        VDP_setTileMapXY(BG_B, TILE(PAL2, base + 3), x, y + i);
        VDP_setTileMapXY(BG_B, TILE(PAL2, base + 5), x + w - 1, y + i);
    }
    VDP_setTileMapXY(BG_B, TILE(PAL2, base + 0), x, y);
    VDP_setTileMapXY(BG_B, TILE(PAL2, base + 2), x + w - 1, y);
    VDP_setTileMapXY(BG_B, TILE(PAL2, base + 6), x, y + h - 1);
    VDP_setTileMapXY(BG_B, TILE(PAL2, base + 8), x + w - 1, y + h - 1);
}

/* The level's background pattern (BITMAP n): 4x4 tiles, palette 0. */
static void background(u8 level) {
    u16 x, y;
    u8 n = (u8)(level - 1);
    if (n == loaded_bg) return;
    VDP_loadTileData(art_bg_tiles[n], TILE_USER_INDEX + BG_BASE, ART_BG_TILES, DMA);
    PAL_setPalette(PAL0, art_bg_pal[n], DMA);
    VDP_setBackgroundColor(0);
    if (loaded_bg == 0xFF)
        for (y = 0; y < 28; ++y)
            for (x = 0; x < 40; ++x)
                VDP_setTileMapXY(BG_B, TILE(PAL0, BG_BASE + (y & 3) * 4 + (x & 3)), x, y);
    loaded_bg = n;
}

static void redraw_background(void) { loaded_bg = 0xFF; }

/* ---- sound ---------------------------------------------------------------- */

static void play_track(s8 t) {
    if (!music_on) t = -1;
    if (t == track) return;
    track = t;
    if (t < 0) XGM2_stop();
    else XGM2_playTrack((u16)t);
}

static s8 track_for(u8 level) { return (s8)((level - 1) / 2 > 4 ? 4 : (level - 1) / 2); }

static void sfx(const u8* s, u32 len, SoundPCMChannel ch) { XGM2_playPCM(s, len, ch); }

/* ---- title screen (the About box) ---------------------------------------- */

static void title_options(void) {
    char v[8];
    static const char* const names[4] = { "Starting Level", "Starting Rows ", "Piece Preview ", "Music         " };
    u8 i;
    for (i = 0; i < 4; ++i) {
        u16 y = 16 + i;
        clear_a(8, y, 24, 1);
        text(8, y, names[i], i == menu_item ? ART_FONT_BLUE : ART_FONT);
        if (i == 0) number(v, start_level);
        else if (i == 1) number(v, start_rows);
        else if (i == 2) strcpy(v, preview ? "On" : "Off");
        else strcpy(v, music_on ? "On" : "Off");
        text(25, y, "<", ART_FONT);
        text(27, y, v, i == menu_item ? ART_FONT_BLUE : ART_FONT);
        text(31, y, ">", ART_FONT);
    }
}

static void title(void) {
    state = TITLE;
    idle = 0;
    VDP_clearPlane(BG_A, TRUE);
    redraw_background();
    background(1);
    box(1, 2, 38, 24, ART_ABOUT, ART_ABOUT + 4);
    text_center(4, "TETRISPACK", ART_FONT);
    text_center(6, "Tetris for Windows 2.00", ART_FONT);
    text_center(7, "on the Sega Genesis", ART_FONT);
    text_center(10, "Original Concept by Alexey Pajitnov", ART_FONT);
    text_center(11, "Windows version by Dave Edson", ART_FONT);
    text_center(12, "github.com/jgordon165/Tetrispack", ART_FONT);
    title_options();
    text_center(23, "Press START", ART_FONT);
    play_track(0);
}

/* ---- the game screen ------------------------------------------------------- */

static void layout(void) {
    u16 x, y;
    VDP_clearPlane(BG_A, TRUE);
    redraw_background();
    background(board.level);
    box(WELL_X - 1, WELL_Y - 1, TC_W + 2, TC_H + 2, ART_FRAME_BLACK, ART_FRAME_BLACK + 4);
    box(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, ART_FRAME_BLACK, ART_FRAME_GRAY);
    box(NEXT_X, NEXT_Y, 6, 6, ART_SUNKEN, ART_SUNKEN + 4);
    text(PANEL_X + 2, PANEL_Y + 1, "Score:", ART_FONT);
    text(PANEL_X + 2, PANEL_Y + 4, "Level:", ART_FONT);
    text(PANEL_X + 2, PANEL_Y + 7, "Lines:", ART_FONT);
    for (y = 0; y < TC_H; ++y)
        for (x = 0; x < TC_W; ++x) shown[y][x] = 0;
    shown_score = 0xFFFFFFFF; shown_lines = 0xFFFF; shown_level = 0xFF; shown_next = 0xFF;
}

static void new_game(u8 demo) {
    tc_new(&board, demo ? 1 : start_level, demo ? 0 : start_rows, (u16)(frame_count * 31 + random() + 1));
    blink = 0;
    demo_planned = 0;
    demo_wait = 0;
    state = demo ? DEMO : PLAY;
    layout();
    play_track(track_for(board.level));
}

static void draw_values(void) {
    char s[12];
    if (board.score != shown_score) {
        shown_score = board.score;
        number(s, board.score);
        clear_a(PANEL_X + 3, PANEL_Y + 2, 7, 1);
        text(PANEL_X + 3, PANEL_Y + 2, s, ART_FONT_BLUE);
    }
    if (board.level != shown_level) {
        shown_level = board.level;
        number(s, board.level);
        clear_a(PANEL_X + 3, PANEL_Y + 5, 3, 1);
        text(PANEL_X + 3, PANEL_Y + 5, s, ART_FONT_BLUE);
        background(board.level);
    }
    if (board.lines != shown_lines) {
        shown_lines = board.lines;
        number(s, board.lines);
        clear_a(PANEL_X + 3, PANEL_Y + 8, 4, 1);
        text(PANEL_X + 3, PANEL_Y + 8, s, ART_FONT_BLUE);
    }
    {
        u8 want = (preview && !board.over) ? board.next : 0;
        if (want != shown_next) {
            /* The next piece on black, in its bounding box, centred. */
            s8 minx = 9, maxx = -9, miny = 9, maxy = -9, cx, cy;
            u8 i;
            u16 ox, oy;
            shown_next = want;
            clear_a(NEXT_X + 1, NEXT_Y + 1, 4, 4);
            if (want) {
                for (i = 0; i < 4; ++i) {
                    cx = tc_cell_x(want, 0, i); cy = tc_cell_y(want, 0, i);
                    if (cx < minx) minx = cx;
                    if (cx > maxx) maxx = cx;
                    if (cy < miny) miny = cy;
                    if (cy > maxy) maxy = cy;
                }
                ox = NEXT_X + 1 + (4 - (maxx - minx + 1)) / 2;
                oy = NEXT_Y + 1 + (4 - (maxy - miny + 1)) / 2;
                VDP_fillTileMapRect(BG_A, TILE(PAL2, ART_FRAME_BLACK + 4), ox, oy, maxx - minx + 1, maxy - miny + 1);
                for (i = 0; i < 4; ++i)
                    VDP_setTileMapXY(BG_A, TILE(PAL1, ART_BLOCK + want - 1),
                                     ox + tc_cell_x(want, 0, i) - minx, oy + tc_cell_y(want, 0, i) - miny);
            }
        }
    }
}

static void draw_well(void) {
    u16 want[TC_H][TC_W];
    u8 x, y, i, inv;
    s8 px, py;
    for (y = 0; y < TC_H; ++y) {
        inv = 0;
        if (blink && (blink & 1))
            for (i = 0; i < board.nclear; ++i) if (board.clearing[i] == y) inv = 1;
        for (x = 0; x < TC_W; ++x) {
            u8 n = board.grid[y][x];
            want[y][x] = (state == PAUSE || !n) ? 0 : TILE(inv ? PAL3 : PAL1, ART_BLOCK + n - 1);
        }
    }
    if (state != PAUSE && !tc_busy(&board))
        for (i = 0; i < 4; ++i) {
            px = (s8)(board.x + tc_cell_x(board.piece, board.rot, i));
            py = (s8)(board.y + tc_cell_y(board.piece, board.rot, i));
            if (py >= 0) want[(u8)py][(u8)px] = TILE(PAL1, ART_BLOCK + board.piece - 1);
        }
    for (y = 0; y < TC_H; ++y)
        for (x = 0; x < TC_W; ++x)
            if (want[y][x] != shown[y][x]) {
                shown[y][x] = want[y][x];
                VDP_setTileMapXY(BG_A, want[y][x], WELL_X + x, WELL_Y + y);
            }
    if (state == PAUSE) text(WELL_X + 2, WELL_Y + 9, "Paused", ART_FONT_WHITE);
    if (state == OVER) {
        text(WELL_X, WELL_Y + 9, "Game Over!", ART_FONT_WHITE);
    }
}

/* ---- input --------------------------------------------------------------- */

static u16 pressed(u16 b) { return (pad & b) && !(pad_prev & b); }

/* Left / Right with the keyboard's typematic feel: a move on the press, then
 * repeats after a delay. */
static u8 repeat(u16 b) {
    if (!(pad & b)) return 0;
    if (!(pad_prev & b)) return 1;
    return held_frames >= DAS_DELAY && ((held_frames - DAS_DELAY) % DAS_RATE) == 0;
}

static void title_input(void) {
    if (pressed(BUTTON_UP)) { menu_item = (u8)((menu_item + 3) % 4); title_options(); }
    if (pressed(BUTTON_DOWN)) { menu_item = (u8)((menu_item + 1) % 4); title_options(); }
    if (pressed(BUTTON_LEFT) || pressed(BUTTON_RIGHT)) {
        s8 d = pressed(BUTTON_RIGHT) ? 1 : -1;
        u8 i;
        if (menu_item == 0) start_level = (u8)((start_level - 1 + 10 + d) % 10 + 1);
        else if (menu_item == 1) {
            for (i = 0; i < 6 && rows_choices[i] != start_rows; ++i) {}
            start_rows = rows_choices[(i + 6 + d) % 6];
        } else if (menu_item == 2) preview = !preview;
        else { music_on = !music_on; track = -2; play_track(0); }
        title_options();
    }
    if (pressed(BUTTON_START) || pressed(BUTTON_A) || pressed(BUTTON_C)) new_game(0);
    else if (pad) idle = 0;
    else if (++idle >= DEMO_AFTER) new_game(1);
}

/* The attract player: plans each piece once, then a move every DEMO_MOVE
 * frames. Any button ends the demo. */
static void demo_input(void) {
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

static void play_input(void) {
    if (pressed(BUTTON_START)) {
        state = PAUSE;
        XGM2_pause();
        return;
    }
    if (tc_busy(&board)) return;
    if (repeat(BUTTON_LEFT)) tc_left(&board);
    if (repeat(BUTTON_RIGHT)) tc_right(&board);
    if (pressed(BUTTON_UP) || pressed(BUTTON_A) || pressed(BUTTON_C)) tc_rotate(&board);
    if (pressed(BUTTON_DOWN) || pressed(BUTTON_B)) tc_drop(&board);
    if (pressed(BUTTON_X)) tc_slide(&board, -1);
    if (pressed(BUTTON_Z)) tc_slide(&board, 1);
}

/* ---- the frame ----------------------------------------------------------- */

static void events(void) {
    u8 e = board.events;
    board.events = 0;
    if (e & TC_EV_ROTATE) sfx(sfx_rotate, sizeof(sfx_rotate), SOUND_PCM_CH2);
    if (e & TC_EV_PLACE) sfx(sfx_place, sizeof(sfx_place), SOUND_PCM_CH2);
    if (e & TC_EV_CLEAR) {
        if (board.nclear >= 3) sfx(sfx_clear_big, sizeof(sfx_clear_big), SOUND_PCM_CH3);
        else sfx(sfx_clear, sizeof(sfx_clear), SOUND_PCM_CH3);
        blink = BLINK_PASSES;
        blink_t = 0;
    }
    if (e & TC_EV_LEVEL) play_track(track_for(board.level));
    if (e & TC_EV_OVER) {
        if (state == DEMO) { title(); return; }
        state = OVER;
        play_track(-1);
    }
}

int main(bool hard) {
    (void)hard;
    VDP_setScreenWidth320();
    VDP_setPlaneSize(64, 32, TRUE);
    JOY_init();
    VDP_loadTileData(art_tiles, TILE_USER_INDEX, ART_TILES, DMA);
    PAL_setPalette(PAL1, art_pal_block, DMA);
    PAL_setPalette(PAL2, art_pal_ui, DMA);
    PAL_setPalette(PAL3, art_pal_invert, DMA);
    XGM2_load(music);
    title();

    for (;;) {
        ++frame_count;
        pad_prev = pad;
        pad = JOY_readJoypad(JOY_1);
        if ((pad & (BUTTON_LEFT | BUTTON_RIGHT)) && (pad & (BUTTON_LEFT | BUTTON_RIGHT)) == (pad_prev & (BUTTON_LEFT | BUTTON_RIGHT)))
            ++held_frames;
        else
            held_frames = 0;

        switch (state) {
        case TITLE:
            title_input();
            break;
        case PLAY:
        case DEMO:
            if (state == DEMO) demo_input(); else play_input();
            if (state != PLAY && state != DEMO) break;
            if (blink) {
                if (++blink_t >= BLINK_FRAMES) {
                    blink_t = 0;
                    if (--blink == 0) tc_collapse(&board);
                }
            } else {
                tc_update(&board, IS_PAL_SYSTEM ? 1200 : 1000);
            }
            events();
            break;
        case PAUSE:
            if (pressed(BUTTON_START)) {
                clear_a(WELL_X, WELL_Y + 9, TC_W, 1);
                state = PLAY;
                XGM2_resume();
            }
            break;
        case OVER:
            if (pressed(BUTTON_START)) title();
            break;
        }
        if (state != TITLE) {
            draw_well();
            draw_values();
        }
        SYS_doVBlankProcess();
    }
    return 0;
}

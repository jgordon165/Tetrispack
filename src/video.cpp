#include "video.h"

#include <cmath>

#include "font_system.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace {

// 3x5 glyphs, rows top to bottom. Drawn for the port; not the original's font.
struct Glyph { char c; const char* rows; };
constexpr Glyph kFont[] = {
    {'0', "111101101101111"}, {'1', "010110010010111"}, {'2', "111001111100111"},
    {'3', "111001111001111"}, {'4', "101101111001001"}, {'5', "111100111001111"},
    {'6', "111100111101111"}, {'7', "111001001001001"}, {'8', "111101111101111"},
    {'9', "111101111001111"}, {'A', "010101111101101"}, {'B', "110101110101110"},
    {'C', "011100100100011"}, {'D', "110101101101110"}, {'E', "111100110100111"},
    {'F', "111100110100100"}, {'G', "011100101101011"}, {'H', "101101111101101"},
    {'I', "111010010010111"}, {'J', "001001001101010"}, {'K', "101101110101101"},
    {'L', "100100100100111"}, {'M', "101111111101101"}, {'N', "110101101101101"},
    {'O', "010101101101010"}, {'P', "110101110100100"}, {'Q', "010101101110011"},
    {'R', "110101110101101"}, {'S', "011100010001110"}, {'T', "111010010010010"},
    {'U', "101101101101111"}, {'V', "101101101101010"}, {'W', "101101111111101"},
    {'X', "101101010101101"}, {'Y', "101101010010010"}, {'Z', "111001010100111"},
    {'-', "000000111000000"}, {':', "000010000010000"}, {'.', "000000000000010"},
    {'!', "010010010000010"}, {'/', "001001010100100"}, {'#', "101111101111101"},
};

const SysGlyph* sysGlyph(char c) {
    for (const auto& g : kSystemFont)
        if (g.c == c) return &g;
    // Letters the font doesn't draw fall back to the other case, then a box.
    const char alt = static_cast<char>(std::isupper(static_cast<unsigned char>(c)) ? std::tolower(static_cast<unsigned char>(c))
                                                                                  : std::toupper(static_cast<unsigned char>(c)));
    for (const auto& g : kSystemFont)
        if (g.c == alt) return &g;
    return nullptr;
}

const char* glyph(char c) {
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    for (const auto& g : kFont)
        if (g.c == c) return g.rows;
    return nullptr;
}

}  // namespace

Video::~Video() {
    for (auto* t : tex_) if (t) SDL_DestroyTexture(t);
    if (canvas_) SDL_DestroyTexture(canvas_);
    if (ren_) SDL_DestroyRenderer(ren_);
    if (win_) SDL_DestroyWindow(win_);
}

bool Video::open(const std::string& title, int canvasW, int canvasH, double scale) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");  // sharp pixels, like the original zoomed
    win_ = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            static_cast<int>(canvasW * scale), static_cast<int>(canvasH * scale), SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!win_) { std::fprintf(stderr, "window: %s\n", SDL_GetError()); return false; }
    ren_ = SDL_CreateRenderer(win_, -1, SDL_RENDERER_PRESENTVSYNC);
    if (!ren_) ren_ = SDL_CreateRenderer(win_, -1, SDL_RENDERER_SOFTWARE);
    if (!ren_) { std::fprintf(stderr, "renderer: %s\n", SDL_GetError()); return false; }
    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
    setCanvas(canvasW, canvasH);
    return true;
}

void Video::setTitle(const std::string& t) { if (win_) SDL_SetWindowTitle(win_, t.c_str()); }

void Video::setCanvas(int w, int h) {
    cw_ = w;
    ch_ = h;
    if (canvas_) SDL_DestroyTexture(canvas_);
    canvas_ = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, w, h);
    if (!canvas_) std::fprintf(stderr, "canvas: %s\n", SDL_GetError());
    SDL_SetRenderTarget(ren_, canvas_);
}

void Video::clear(uint8_t r, uint8_t g, uint8_t b) {
    SDL_SetRenderDrawColor(ren_, r, g, b, 255);
    SDL_RenderClear(ren_);
}

void Video::fill(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    SDL_SetRenderDrawColor(ren_, r, g, b, a);
    const SDL_Rect rc{x, y, w, h};
    SDL_RenderFillRect(ren_, &rc);
}

int Video::textWidth(const std::string& s, int scale, Font font) {
    if (s.empty()) return 0;
    if (font == Font::Small) return static_cast<int>(s.size()) * 4 * scale - scale;
    int w = 0;
    for (char c : s) {
        const SysGlyph* g = sysGlyph(c);
        w += ((g ? g->width : 5) + 1) * scale;
    }
    return w - scale;
}

void Video::text(int x, int y, const std::string& s, int scale, uint8_t r, uint8_t g, uint8_t b, Font font) {
    SDL_SetRenderDrawColor(ren_, r, g, b, 255);
    if (font == Font::System) {
        for (char c : s) {
            const SysGlyph* gl = sysGlyph(c);
            const int w = gl ? gl->width : 5;
            for (int row = 0; row < 13; ++row) {
                const char* bits = gl ? gl->rows[row] : (row == 0 || row == 9 ? "#####" : row < 9 ? "#...#" : "");
                if (!bits) continue;
                for (int col = 0; bits[col] && col < w; ++col) {
                    if (bits[col] != '#') continue;
                    const SDL_Rect px{x + col * scale, y + row * scale, scale, scale};
                    SDL_RenderFillRect(ren_, &px);
                }
            }
            x += (w + 1) * scale;
        }
        return;
    }
    for (char c : s) {
        if (const char* rows = glyph(c)) {
            for (int i = 0; i < 15; ++i) {
                if (rows[i] != '1') continue;
                const SDL_Rect px{x + (i % 3) * scale, y + (i / 3) * scale, scale, scale};
                SDL_RenderFillRect(ren_, &px);
            }
        }
        x += 4 * scale;
    }
}

int Video::addImage(const tp::Image& img) {
    SDL_Texture* t = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, img.width, img.height);
    if (!t) { std::fprintf(stderr, "texture: %s\n", SDL_GetError()); return -1; }
    SDL_UpdateTexture(t, nullptr, img.rgba.data(), img.width * 4);
    tex_.push_back(t);
    return static_cast<int>(tex_.size()) - 1;
}

void Video::draw(int handle, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh) {
    if (handle < 0 || handle >= static_cast<int>(tex_.size())) return;
    const SDL_Rect src{sx, sy, sw, sh}, dst{dx, dy, dw, dh};
    SDL_RenderCopy(ren_, tex_[handle], &src, &dst);
}

// The canvas, scaled to fit the window with its aspect kept (letterboxed).
void Video::compose() {
    SDL_SetRenderTarget(ren_, nullptr);
    SDL_SetRenderDrawColor(ren_, 0, 0, 0, 255);
    SDL_RenderClear(ren_);
    int ow = 0, oh = 0;
    SDL_GetRendererOutputSize(ren_, &ow, &oh);
    const double s = std::min(double(ow) / cw_, double(oh) / ch_);
    const int dw = static_cast<int>(cw_ * s + 0.5), dh = static_cast<int>(ch_ * s + 0.5);
    const SDL_Rect dst{(ow - dw) / 2, (oh - dh) / 2, dw, dh};
    SDL_RenderCopy(ren_, canvas_, nullptr, &dst);
}

bool Video::windowToCanvas(int wx, int wy, int& cx, int& cy) const {
    int ow = 0, oh = 0, ww = 0, wh = 0;
    SDL_GetRendererOutputSize(ren_, &ow, &oh);
    SDL_GetWindowSize(win_, &ww, &wh);
    if (ow <= 0 || oh <= 0 || ww <= 0 || wh <= 0 || cw_ <= 0 || ch_ <= 0) return false;
    // Window points to output pixels (they differ on high-DPI displays).
    const double px = wx * double(ow) / ww, py = wy * double(oh) / wh;
    const double s = std::min(double(ow) / cw_, double(oh) / ch_);
    const double dw = cw_ * s, dh = ch_ * s;
    const double x = (px - (ow - dw) / 2) / s, y = (py - (oh - dh) / 2) / s;
    cx = static_cast<int>(std::floor(x));
    cy = static_cast<int>(std::floor(y));
    return cx >= 0 && cy >= 0 && cx < cw_ && cy < ch_;
}

void Video::present() {
    SDL_RenderPresent(ren_);
    SDL_SetRenderTarget(ren_, canvas_);
}

bool Video::screenshot(const std::string& path) {
    // The window as shown (the canvas scaled to it).
    int ow = 0, oh = 0;
    SDL_GetRendererOutputSize(ren_, &ow, &oh);
    SDL_Surface* full = SDL_CreateRGBSurfaceWithFormat(0, ow, oh, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!full) return false;
    SDL_RenderReadPixels(ren_, nullptr, SDL_PIXELFORMAT_ARGB8888, full->pixels, full->pitch);
    const bool ok = SDL_SaveBMP(full, path.c_str()) == 0;
    SDL_FreeSurface(full);
    return ok;
}

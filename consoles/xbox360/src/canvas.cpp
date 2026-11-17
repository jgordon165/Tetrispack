#include "canvas.h"

#include "font_small.h"
#include "font_system.h"

#include <cctype>
#include <cstdio>

namespace x360 {

namespace {

const SysGlyph* sysGlyph(char c) {
    const size_t n = sizeof(kSystemFont) / sizeof(kSystemFont[0]);
    for (size_t i = 0; i < n; ++i)
        if (kSystemFont[i].c == c) return &kSystemFont[i];
    // Letters the font doesn't draw fall back to the other case, then a box.
    const unsigned char u = (unsigned char)c;
    const char alt = char(std::isupper(u) ? std::tolower(u) : std::toupper(u));
    for (size_t i = 0; i < n; ++i)
        if (kSystemFont[i].c == alt) return &kSystemFont[i];
    return 0;
}

const char* smallGlyph(char c) {
    const unsigned char u = (unsigned char)std::toupper((unsigned char)c);
    for (size_t i = 0; i < sizeof(kFont) / sizeof(kFont[0]); ++i)
        if ((unsigned char)kFont[i].c == u) return kFont[i].rows;
    return 0;
}

uint32_t pack(uint8_t r, uint8_t g, uint8_t b) { return 0xFF000000u | (uint32_t(r) << 16) | (uint32_t(g) << 8) | b; }

}  // namespace

void Canvas::resize(int w, int h) {
    w_ = w > 0 ? w : 1;
    h_ = h > 0 ? h : 1;
    px_.assign(size_t(w_) * h_, 0xFF000000u);
}

void Canvas::clear(uint8_t r, uint8_t g, uint8_t b) { px_.assign(px_.size(), pack(r, g, b)); }

void Canvas::fill(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > w_ ? w_ : x + w, y1 = y + h > h_ ? h_ : y + h;
    if (x0 >= x1 || y0 >= y1 || a == 0) return;
    const uint32_t c = pack(r, g, b);
    for (int yy = y0; yy < y1; ++yy) {
        uint32_t* row = &px_[size_t(yy) * w_];
        for (int xx = x0; xx < x1; ++xx) {
            if (a == 255) { row[xx] = c; continue; }
            const uint32_t d = row[xx];
            const unsigned dr = (d >> 16) & 255, dg = (d >> 8) & 255, db = d & 255;
            row[xx] = pack(uint8_t((r * a + dr * (255 - a)) / 255), uint8_t((g * a + dg * (255 - a)) / 255),
                           uint8_t((b * a + db * (255 - a)) / 255));
        }
    }
}

int Canvas::textWidth(const std::string& s, int scale, Font font) {
    if (s.empty()) return 0;
    if (font == Small) return int(s.size()) * 4 * scale - scale;
    int w = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        const SysGlyph* g = sysGlyph(s[i]);
        w += ((g ? g->width : 5) + 1) * scale;
    }
    return w - scale;
}

void Canvas::text(int x, int y, const std::string& s, int scale, uint8_t r, uint8_t g, uint8_t b, Font font) {
    for (size_t i = 0; i < s.size(); ++i) {
        if (font == System) {
            const SysGlyph* gl = sysGlyph(s[i]);
            const int w = gl ? gl->width : 5;
            for (int row = 0; row < 13; ++row) {
                const char* bits = gl ? gl->rows[row] : (row == 0 || row == 9 ? "#####" : row < 9 ? "#...#" : "");
                if (!bits) continue;
                for (int col = 0; bits[col] && col < w; ++col)
                    if (bits[col] == '#') fill(x + col * scale, y + row * scale, scale, scale, r, g, b, 255);
            }
            x += (w + 1) * scale;
        } else {
            if (const char* rows = smallGlyph(s[i]))
                for (int k = 0; k < 15; ++k)
                    if (rows[k] == '1') fill(x + (k % 3) * scale, y + (k / 3) * scale, scale, scale, r, g, b, 255);
            x += 4 * scale;
        }
    }
}

int Canvas::addImage(const Image& img) {
    images_.push_back(img);
    return int(images_.size()) - 1;
}

void Canvas::draw(int handle, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh) {
    if (handle < 0 || handle >= int(images_.size()) || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;
    const Image& im = images_[size_t(handle)];
    for (int y = 0; y < dh; ++y) {
        const int ty = dy + y;
        if (ty < 0 || ty >= h_) continue;
        const int iy = sy + y * sh / dh;
        if (iy < 0 || iy >= im.height) continue;
        for (int x = 0; x < dw; ++x) {
            const int tx = dx + x;
            if (tx < 0 || tx >= w_) continue;
            const int ix = sx + x * sw / dw;
            if (ix < 0 || ix >= im.width) continue;
            px_[size_t(ty) * w_ + tx] = im.argb[size_t(iy) * im.width + ix];
        }
    }
}

bool Canvas::saveBmp(const std::string& path) const {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const unsigned stride = (unsigned(w_) * 3 + 3) & ~3u, size = 54 + stride * unsigned(h_);
    unsigned char hdr[54] = { 'B', 'M' };
    unsigned char* p = hdr;
    unsigned vals[][2] = { { 2, size }, { 10, 54 }, { 14, 40 }, { 18, unsigned(w_) }, { 22, unsigned(h_) }, { 34, stride * unsigned(h_) } };
    for (size_t i = 0; i < sizeof(vals) / sizeof(vals[0]); ++i)
        for (int k = 0; k < 4; ++k) p[vals[i][0] + k] = (unsigned char)(vals[i][1] >> (8 * k));
    hdr[26] = 1;
    hdr[28] = 24;
    std::fwrite(hdr, 1, 54, f);
    std::vector<unsigned char> line(stride, 0);
    for (int y = h_ - 1; y >= 0; --y) {
        for (int x = 0; x < w_; ++x) {
            const uint32_t c = px_[size_t(y) * w_ + x];
            line[x * 3] = (unsigned char)(c & 255);
            line[x * 3 + 1] = (unsigned char)((c >> 8) & 255);
            line[x * 3 + 2] = (unsigned char)((c >> 16) & 255);
        }
        std::fwrite(&line[0], 1, stride, f);
    }
    return std::fclose(f) == 0;
}

}  // namespace x360

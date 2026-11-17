// The game's canvas for the Xbox 360 host: drawn in software into 32-bit
// pixels (0xAARRGGBB), then shown by the platform layer (a Direct3D 9
// texture on the console, a .bmp in the headless test). The same drawing
// calls as the PC's SDL renderer (src/video.cpp): fills, the two built-in
// fonts, and bitmaps scaled with nearest-neighbour sampling.
#pragma once

#include "formats360.h"

#include <stdint.h>
#include <string>
#include <vector>

namespace x360 {

class Canvas {
public:
    enum Font { Small, System };

    Canvas() : w_(0), h_(0) {}
    void resize(int w, int h);
    int width() const { return w_; }
    int height() const { return h_; }
    const uint32_t* pixels() const { return px_.empty() ? 0 : &px_[0]; }

    void clear(uint8_t r, uint8_t g, uint8_t b);
    void fill(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void text(int x, int y, const std::string& s, int scale, uint8_t r, uint8_t g, uint8_t b, Font font);
    static int textWidth(const std::string& s, int scale, Font font);

    int addImage(const Image& img);   // a handle for draw()
    void draw(int handle, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh);

    // The canvas as a 24-bit .bmp (tests and screenshots).
    bool saveBmp(const std::string& path) const;

private:
    int w_, h_;
    std::vector<uint32_t> px_;
    std::vector<Image> images_;
};

}  // namespace x360

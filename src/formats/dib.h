// Device-independent bitmaps as stored in BITMAP resources (no file header):
// a BITMAPINFOHEADER (Windows 3.x) or BITMAPCOREHEADER (OS/2 1.x), the
// palette, then the bottom-up pixel rows. Decoded to 32-bit RGBA.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace tp {

struct Image {
    int width = 0, height = 0;
    std::vector<uint32_t> rgba;  // row-major, top row first, see packRGBA (SDL_PIXELFORMAT_RGBA32 on little-endian)

    uint32_t at(int x, int y) const { return rgba[static_cast<size_t>(y) * width + x]; }
};

// Packs a colour the way Image stores it (bytes R, G, B, A in memory).
constexpr uint32_t packRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return static_cast<uint32_t>(r) | (static_cast<uint32_t>(g) << 8) |
           (static_cast<uint32_t>(b) << 16) | (static_cast<uint32_t>(a) << 24);
}

// Supports 1, 4, 8 and 24 bits per pixel, uncompressed (BI_RGB), plus RLE8
// and RLE4. Throws FormatError otherwise.
Image decodeDib(std::span<const uint8_t> dib);

}  // namespace tp

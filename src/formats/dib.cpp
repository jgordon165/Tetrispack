#include "formats/dib.h"

#include "formats/binreader.h"

namespace tp {

namespace {

constexpr uint32_t BI_RGB = 0, BI_RLE8 = 1, BI_RLE4 = 2;

// RLE8/RLE4 into one palette index per pixel, bottom-up rows.
std::vector<uint8_t> unRle(BinReader& r, size_t start, int w, int h, bool four) {
    std::vector<uint8_t> idx(static_cast<size_t>(w) * h, 0);
    int x = 0, y = 0;
    auto put = [&](uint8_t v) {
        if (x < w && y < h) idx[static_cast<size_t>(y) * w + x] = v;
        ++x;
    };
    r.seek(start);
    while (r.tell() + 1 < r.size()) {
        const uint8_t n = r.u8(), v = r.u8();
        if (n) {
            for (int i = 0; i < n; ++i) put(four ? ((i & 1) ? (v & 0xF) : (v >> 4)) : v);
            continue;
        }
        if (v == 0) { x = 0; ++y; continue; }   // end of line
        if (v == 1) break;                      // end of bitmap
        if (v == 2) { x += r.u8(); y += r.u8(); continue; }  // delta
        // Absolute run of v pixels, padded to a 16-bit boundary.
        const size_t bytes = four ? (v + 1u) / 2u : v;
        for (int i = 0; i < v; ++i) {
            if (four) { const uint8_t b = r.u8At(r.tell() + i / 2); put((i & 1) ? (b & 0xF) : (b >> 4)); }
            else put(r.u8At(r.tell() + i));
        }
        r.skip(bytes + (bytes & 1));
    }
    return idx;
}

}  // namespace

Image decodeDib(std::span<const uint8_t> dib) {
    BinReader r(dib);
    const uint32_t hdr = r.u32At(0);
    int w, h, bpp;
    uint32_t compression = BI_RGB, clrUsed = 0;
    size_t palEntry;
    if (hdr == 12) {  // BITMAPCOREHEADER
        w = r.u16At(4);
        h = static_cast<int16_t>(r.u16At(6));
        bpp = r.u16At(10);
        palEntry = 3;
    } else if (hdr >= 40) {  // BITMAPINFOHEADER (or a later, longer one)
        w = static_cast<int32_t>(r.u32At(4));
        h = static_cast<int32_t>(r.u32At(8));
        bpp = r.u16At(14);
        compression = r.u32At(16);
        clrUsed = r.u32At(32);
        palEntry = 4;
    } else {
        throw FormatError("unknown bitmap header size " + std::to_string(hdr));
    }
    const bool topDown = h < 0;
    if (topDown) h = -h;
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192) throw FormatError("bad bitmap size");
    if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 24) throw FormatError("unsupported bit depth " + std::to_string(bpp));

    std::vector<uint32_t> pal;
    if (bpp <= 8) {
        const uint32_t n = clrUsed ? clrUsed : (1u << bpp);
        if (n > 256) throw FormatError("bad palette size");
        for (uint32_t i = 0; i < n; ++i) {
            const size_t o = hdr + i * palEntry;
            pal.push_back(packRGBA(r.u8At(o + 2), r.u8At(o + 1), r.u8At(o)));  // stored B, G, R
        }
    }
    const size_t bits = hdr + pal.size() * palEntry;
    auto color = [&](uint32_t i) { return i < pal.size() ? pal[i] : packRGBA(0, 0, 0); };

    Image img;
    img.width = w;
    img.height = h;
    img.rgba.resize(static_cast<size_t>(w) * h);

    if (compression == BI_RLE8 || compression == BI_RLE4) {
        if (bpp != (compression == BI_RLE8 ? 8 : 4)) throw FormatError("RLE with wrong bit depth");
        const auto idx = unRle(r, bits, w, h, compression == BI_RLE4);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                img.rgba[static_cast<size_t>(h - 1 - y) * w + x] = color(idx[static_cast<size_t>(y) * w + x]);
        return img;
    }
    if (compression != BI_RGB) throw FormatError("unsupported bitmap compression " + std::to_string(compression));

    const size_t stride = ((static_cast<size_t>(w) * bpp + 31) / 32) * 4;
    for (int row = 0; row < h; ++row) {
        const auto line = r.bytes(bits + row * stride, stride);
        const int y = topDown ? row : h - 1 - row;
        uint32_t* out = &img.rgba[static_cast<size_t>(y) * w];
        for (int x = 0; x < w; ++x) {
            switch (bpp) {
                case 1: out[x] = color((line[x / 8] >> (7 - x % 8)) & 1); break;
                case 4: out[x] = color((line[x / 2] >> ((x & 1) ? 0 : 4)) & 0xF); break;
                case 8: out[x] = color(line[x]); break;
                default: out[x] = packRGBA(line[x * 3 + 2], line[x * 3 + 1], line[x * 3]); break;
            }
        }
    }
    return img;
}

}  // namespace tp

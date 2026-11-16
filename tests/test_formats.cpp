// Data-free tests of the NE and bitmap readers, on executables built here in
// memory. With TETRISPACK_DATA set (folder with TETRIS.EXE), also parses the
// real file.

#include "formats/dib.h"
#include "formats/ne.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

using Bytes = std::vector<uint8_t>;

static void put16(Bytes& b, size_t at, uint16_t v) { b[at] = v & 0xFF; b[at + 1] = v >> 8; }
static void put32(Bytes& b, size_t at, uint32_t v) { for (int i = 0; i < 4; ++i) b[at + i] = (v >> (8 * i)) & 0xFF; }
static void app16(Bytes& b, uint16_t v) { b.push_back(v & 0xFF); b.push_back(v >> 8); }
static void app32(Bytes& b, uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back((v >> (8 * i)) & 0xFF); }
static void appP(Bytes& b, const std::string& s) { b.push_back(static_cast<uint8_t>(s.size())); b.insert(b.end(), s.begin(), s.end()); }

// A 4bpp 3x2 bitmap: top row palette 1,2,3; bottom row 4,5,6.
static Bytes dib4() {
    Bytes b(40, 0);
    put32(b, 0, 40); put32(b, 4, 3); put32(b, 8, 2); put16(b, 12, 1); put16(b, 14, 4);
    put32(b, 32, 7);  // 7 colours used
    for (uint8_t i = 0; i < 7; ++i) { b.push_back(i * 10); b.push_back(i * 20); b.push_back(i * 30); b.push_back(0); }  // B G R 0
    // Bottom-up rows, 4-byte stride: bottom row first.
    const uint8_t rows[2][4] = {{0x45, 0x60, 0, 0}, {0x12, 0x30, 0, 0}};
    for (auto& r : rows) b.insert(b.end(), r, r + 4);
    return b;
}

// A 1bpp 8x1 core-header bitmap, alternating.
static Bytes dib1core() {
    Bytes b(12, 0);
    put32(b, 0, 12); put16(b, 4, 8); put16(b, 6, 1); put16(b, 8, 1); put16(b, 10, 1);
    const uint8_t pal[] = {0, 0, 0, 255, 255, 255};  // black, white (B G R)
    b.insert(b.end(), pal, pal + 6);
    const uint8_t row[] = {0xAA, 0, 0, 0};
    b.insert(b.end(), row, row + 4);
    return b;
}

// An RLE8 4x2 bitmap.
static Bytes dibRle8() {
    Bytes b(40, 0);
    put32(b, 0, 40); put32(b, 4, 4); put32(b, 8, 2); put16(b, 12, 1); put16(b, 14, 8);
    put32(b, 16, 1); put32(b, 32, 3);
    const uint8_t pal[] = {0, 0, 0, 0, 0, 0, 255, 0, 0, 255, 0, 0};  // black, red, green
    b.insert(b.end(), pal, pal + sizeof pal);
    const uint8_t data[] = {4, 1, 0, 0,               // bottom row: 4x red, end of line
                            0, 4, 2, 2, 1, 1, 0, 1};  // top row: absolute 2,2,1,1; end of bitmap
    b.insert(b.end(), data, data + sizeof data);
    return b;
}

static Bytes stringBlock(const std::vector<std::string>& s) {
    Bytes b;
    for (unsigned i = 0; i < 16; ++i) appP(b, i < s.size() ? s[i] : "");
    return b;
}

// MZ stub + NE header + resource table + resident names, then the resources.
static Bytes buildNe() {
    const Bytes bmpA = dib4(), bmpB = dib1core(), bmpC = dibRle8();
    const Bytes strs = stringBlock({"", "Tetris", "Game Over"});  // ids 0..15: 1 and 2 set
    const uint16_t shift = 4;

    Bytes rt;  // resource table, offsets relative to its start
    app16(rt, shift);
    app16(rt, 0x8002); app16(rt, 3); app32(rt, 0);
    const size_t bmpEntries = rt.size();
    for (int i = 0; i < 3; ++i) { for (int k = 0; k < 12; ++k) rt.push_back(0); }
    app16(rt, 0x8006); app16(rt, 1); app32(rt, 0);
    const size_t strEntry = rt.size();
    for (int k = 0; k < 12; ++k) rt.push_back(0);
    app16(rt, 0);
    const size_t nameOff = rt.size();
    appP(rt, "Blocks");
    rt.push_back(0);

    Bytes out(0x40, 0);
    out[0] = 'M'; out[1] = 'Z';
    put32(out, 0x3C, 0x40);
    Bytes ne(0x40, 0);
    ne[0] = 'N'; ne[1] = 'E';
    put16(ne, 0x24, 0x40);
    put16(ne, 0x26, static_cast<uint16_t>(0x40 + rt.size()));
    put16(ne, 0x3E, 0x0300);  // Windows 3.0
    out.insert(out.end(), ne.begin(), ne.end());
    const size_t rtAt = out.size();
    out.insert(out.end(), rt.begin(), rt.end());
    appP(out, "TETRIS"); app16(out, 0); out.push_back(0);

    auto place = [&](const Bytes& data, size_t entry, uint16_t id) {
        while (out.size() % (1u << shift)) out.push_back(0);
        const size_t at = out.size();
        out.insert(out.end(), data.begin(), data.end());
        const size_t e = rtAt + entry;
        put16(out, e + 0, static_cast<uint16_t>(at >> shift));
        put16(out, e + 2, static_cast<uint16_t>((data.size() + (1u << shift) - 1) >> shift));
        put16(out, e + 4, 0x30);
        put16(out, e + 6, id);
    };
    place(bmpA, bmpEntries + 0, 0x8000 | 101);
    place(bmpB, bmpEntries + 12, static_cast<uint16_t>(nameOff));
    place(bmpC, bmpEntries + 24, 0x8000 | 102);
    place(strs, strEntry, 0x8000 | 1);
    while (out.size() % 16) out.push_back(0);
    return out;
}

static void testNe() {
    tp::NeFile ne(buildNe());
    CHECK(ne.moduleName() == "TETRIS");
    CHECK(ne.expectedWindowsMajor() == 3 && ne.expectedWindowsMinor() == 0);
    CHECK(ne.resources().size() == 4);

    const auto* a = ne.find(2, 101);
    CHECK(a != nullptr);
    if (a) {
        const auto img = tp::decodeDib(ne.read(*a));
        CHECK(img.width == 3 && img.height == 2);
        CHECK(img.at(0, 0) == tp::packRGBA(30, 20, 10));    // palette 1, top-left
        CHECK(img.at(2, 0) == tp::packRGBA(90, 60, 30));    // palette 3
        CHECK(img.at(0, 1) == tp::packRGBA(120, 80, 40));   // palette 4, bottom-left
    }
    const auto* b = ne.find(2, std::string("blocks"));  // names are case-insensitive
    CHECK(b != nullptr && b->key.name == "BLOCKS");
    if (b) {
        const auto img = tp::decodeDib(ne.read(*b));
        CHECK(img.width == 8 && img.height == 1);
        CHECK(img.at(0, 0) == tp::packRGBA(255, 255, 255));
        CHECK(img.at(1, 0) == tp::packRGBA(0, 0, 0));
    }
    CHECK(ne.find(2, std::string("#102")) != nullptr);
    if (const auto* c = ne.find(2, 102)) {
        const auto img = tp::decodeDib(ne.read(*c));
        CHECK(img.width == 4 && img.height == 2);
        CHECK(img.at(0, 1) == tp::packRGBA(255, 0, 0));
        CHECK(img.at(3, 1) == tp::packRGBA(255, 0, 0));
        CHECK(img.at(0, 0) == tp::packRGBA(0, 255, 0));
        CHECK(img.at(3, 0) == tp::packRGBA(255, 0, 0));
    }
    CHECK(ne.find(2, 999) == nullptr);
    CHECK(ne.string(1).value_or("") == "Tetris");
    CHECK(ne.string(2).value_or("") == "Game Over");
    CHECK(!ne.string(3).has_value());
    CHECK(!ne.string(40).has_value());
}

static void testRejects() {
    bool threw = false;
    try { tp::NeFile(Bytes(10, 0)); } catch (const tp::FormatError&) { threw = true; }
    CHECK(threw);

    // Truncated: cut the file inside the resource table.
    Bytes cut = buildNe();
    cut.resize(0x90);
    threw = false;
    try { tp::NeFile ne(cut); } catch (const tp::FormatError&) { threw = true; }
    CHECK(threw);

    // A PE (32-bit) executable is not NE.
    Bytes pe(0x100, 0);
    pe[0] = 'M'; pe[1] = 'Z'; pe[0x3C] = 0x80; pe[0x80] = 'P'; pe[0x81] = 'E';
    threw = false;
    try { tp::NeFile ne(pe); } catch (const tp::FormatError&) { threw = true; }
    CHECK(threw);

    Bytes badDib(40, 0);
    put32(badDib, 0, 40); put32(badDib, 4, 4); put32(badDib, 8, 4); put16(badDib, 14, 16);
    threw = false;
    try { tp::decodeDib(badDib); } catch (const tp::FormatError&) { threw = true; }
    CHECK(threw);
}

static void testRealData() {
    const char* dir = std::getenv("TETRISPACK_DATA");
    if (!dir) return;
    namespace fs = std::filesystem;
    for (const auto& e : fs::directory_iterator(dir)) {
        std::string n = e.path().filename().string();
        for (auto& c : n) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (n != "TETRIS.EXE") continue;
        const auto ne = tp::NeFile::load(e.path().string());
        std::printf("[data] %s: module %s, %zu resources\n", e.path().string().c_str(), ne.moduleName().c_str(),
                    ne.resources().size());
        CHECK(!ne.resources().empty());
        for (const auto& r : ne.resources()) {
            if (r.typeId != 2) continue;
            try { tp::decodeDib(ne.read(r)); } catch (const std::exception& ex) {
                std::fprintf(stderr, "[data] bitmap %s: %s\n", r.key.str().c_str(), ex.what());
                ++failures;
            }
        }
        return;
    }
    std::fprintf(stderr, "[data] TETRIS.EXE not found in %s\n", dir);
    ++failures;
}

int main() {
    testNe();
    testRejects();
    testRealData();
    if (failures) { std::fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    std::printf("formats: all passed\n");
    return 0;
}

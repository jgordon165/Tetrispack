// The original's files for the Xbox 360 host, in C++03 for the XDK's
// Visual C++ 2010 compiler: the NE executable's resources (TETRIS.EXE,
// ABOUTTET.DLL), its bitmaps and dialog templates. The same formats as
// src/formats/ (see docs/formats.md), read byte by byte so the 360's
// big-endian PowerPC gets the same values as a PC.
#pragma once

#include <stdint.h>
#include <string>
#include <vector>

namespace x360 {

struct Image {
    int width, height;
    std::vector<uint32_t> argb;   // 0xAARRGGBB, top row first
    Image() : width(0), height(0) {}
};

struct Resource {
    uint16_t typeId;   // 0 when the type is named
    std::string typeName;
    uint16_t id;       // 0 when named
    std::string name;
    uint32_t offset, length;
};

struct DialogControl {
    int x, y, w, h, id;
    uint32_t style;
    std::string cls, text;
};

struct Dialog {
    int x, y, w, h;
    uint32_t style;
    std::string caption;
    std::vector<DialogControl> controls;
};

const char* resTypeName(uint16_t type);

class NeFile {
public:
    // False (and error() says why) when `data` isn't an NE executable.
    bool load(const std::vector<uint8_t>& data);
    bool loaded() const { return !data_.empty(); }
    const std::string& error() const { return error_; }

    const std::string& moduleName() const { return module_; }
    int windowsMajor() const { return winMajor_; }
    int windowsMinor() const { return winMinor_; }
    const std::vector<Resource>& resources() const { return res_; }

    const Resource* find(uint16_t type, uint16_t id) const;
    const Resource* find(uint16_t type, const std::string& name) const;   // "#12" by number
    bool read(const Resource& r, std::vector<uint8_t>& out) const;
    bool string(uint16_t id, std::string& out) const;
    bool cstringStartingWith(const std::string& prefix, std::string& out) const;

private:
    std::vector<uint8_t> data_;
    std::string module_, error_;
    int winMajor_, winMinor_;
    std::vector<Resource> res_;
};

// BITMAP resources (BITMAPINFOHEADER / BITMAPCOREHEADER, 1/4/8/24 bpp, RLE4/8).
bool decodeDib(const std::vector<uint8_t>& dib, Image& out, std::string& error);
// DIALOG resources (16-bit DLGTEMPLATE).
bool decodeDialog(const std::vector<uint8_t>& data, Dialog& out);

}  // namespace x360

#include "formats360.h"

#include <cctype>
#include <cstdlib>

namespace x360 {

namespace {

// Bounds-checked little-endian reads; `ok` goes false on any read past the end.
struct Reader {
    const uint8_t* p;
    size_t n;
    bool ok;
    Reader(const std::vector<uint8_t>& v) : p(v.empty() ? 0 : &v[0]), n(v.size()), ok(true) {}
    uint8_t u8(size_t o) { if (o >= n) { ok = false; return 0; } return p[o]; }
    uint16_t u16(size_t o) { return uint16_t(u8(o) | (u8(o + 1) << 8)); }
    uint32_t u32(size_t o) { return uint32_t(u16(o)) | (uint32_t(u16(o + 2)) << 16); }
    std::string pstring(size_t o) {
        const size_t len = u8(o);
        std::string s;
        for (size_t i = 0; i < len; ++i) s += char(u8(o + 1 + i));
        return s;
    }
    std::string cstring(size_t& o) {
        std::string s;
        for (;;) {
            const uint8_t c = u8(o++);
            if (!ok || c == 0) break;
            s += char(c);
        }
        return s;
    }
};

std::string upper(std::string s) {
    for (size_t i = 0; i < s.size(); ++i) s[i] = char(std::toupper((unsigned char)s[i]));
    return s;
}

std::string number(unsigned v) {
    char buf[16];
    int n = 0;
    do { buf[n++] = char('0' + v % 10); v /= 10; } while (v);
    std::string s;
    while (n) s += buf[--n];
    return s;
}

uint32_t argb(uint8_t r, uint8_t g, uint8_t b) { return 0xFF000000u | (uint32_t(r) << 16) | (uint32_t(g) << 8) | b; }

}  // namespace

const char* resTypeName(uint16_t type) {
    switch (type) {
        case 1: return "CURSOR";
        case 2: return "BITMAP";
        case 3: return "ICON";
        case 4: return "MENU";
        case 5: return "DIALOG";
        case 6: return "STRING";
        case 7: return "FONTDIR";
        case 8: return "FONT";
        case 9: return "ACCELERATOR";
        case 10: return "RCDATA";
        case 12: return "GROUP_CURSOR";
        case 14: return "GROUP_ICON";
        case 16: return "VERSION";
    }
    return "?";
}

bool NeFile::load(const std::vector<uint8_t>& data) {
    data_.clear();
    res_.clear();
    Reader r(data);
    if (data.size() < 0x40 || r.u16(0) != 0x5A4D) { error_ = "not an MZ executable"; return false; }
    const size_t ne = r.u32(0x3C);
    if (r.u16(ne) != 0x454E) { error_ = "not a 16-bit Windows (NE) executable"; return false; }
    const size_t tab = ne + r.u16(ne + 0x24), names = ne + r.u16(ne + 0x26);
    const uint16_t ver = r.u16(ne + 0x3E);
    winMajor_ = ver >> 8;
    winMinor_ = ver & 0xFF;
    module_ = r.pstring(names);
    if (tab != names) {
        const unsigned shift = r.u16(tab);
        if (shift > 16) { error_ = "bad resource alignment"; return false; }
        size_t pos = tab + 2;
        for (;;) {
            if (pos >= names || !r.ok) { error_ = "resource table not terminated"; return false; }
            const uint16_t type = r.u16(pos);
            if (type == 0) break;
            const uint16_t count = r.u16(pos + 2);
            pos += 8;
            for (uint16_t i = 0; i < count; ++i, pos += 12) {
                Resource res;
                res.typeId = (type & 0x8000) ? uint16_t(type & 0x7FFF) : 0;
                if (!(type & 0x8000)) res.typeName = upper(r.pstring(tab + type));
                res.offset = uint32_t(r.u16(pos)) << shift;
                res.length = uint32_t(r.u16(pos + 2)) << shift;
                const uint16_t id = r.u16(pos + 6);
                res.id = (id & 0x8000) ? uint16_t(id & 0x7FFF) : 0;
                if (!(id & 0x8000)) res.name = upper(r.pstring(tab + id));
                if (res.offset > data.size()) { error_ = "resource past the end of the file"; return false; }
                if (res.length > data.size() - res.offset) res.length = uint32_t(data.size() - res.offset);
                res_.push_back(res);
            }
        }
    }
    if (!r.ok) { error_ = "truncated executable"; return false; }
    data_ = data;
    return true;
}

const Resource* NeFile::find(uint16_t type, uint16_t id) const {
    for (size_t i = 0; i < res_.size(); ++i)
        if (res_[i].typeId == type && res_[i].name.empty() && res_[i].id == id) return &res_[i];
    return 0;
}

const Resource* NeFile::find(uint16_t type, const std::string& name) const {
    if (!name.empty() && name[0] == '#') return find(type, uint16_t(std::atoi(name.c_str() + 1)));
    const std::string want = upper(name);
    for (size_t i = 0; i < res_.size(); ++i)
        if (res_[i].typeId == type && res_[i].name == want) return &res_[i];
    return 0;
}

bool NeFile::read(const Resource& r, std::vector<uint8_t>& out) const {
    if (r.offset > data_.size() || r.length > data_.size() - r.offset) return false;
    out.assign(data_.begin() + r.offset, data_.begin() + r.offset + r.length);
    return true;
}

bool NeFile::string(uint16_t id, std::string& out) const {
    const Resource* block = find(6, uint16_t(id / 16 + 1));
    std::vector<uint8_t> bytes;
    if (!block || !read(*block, bytes)) return false;
    Reader r(bytes);
    size_t pos = 0;
    for (unsigned i = 0; i < 16; ++i) {
        const std::string s = r.pstring(pos);
        if (!r.ok) return false;
        pos += 1 + s.size();
        if (i == id % 16u) {
            out = s;
            return !s.empty();
        }
    }
    return false;
}

bool NeFile::cstringStartingWith(const std::string& prefix, std::string& out) const {
    if (prefix.empty() || data_.size() < prefix.size()) return false;
    for (size_t i = 0; i + prefix.size() <= data_.size(); ++i) {
        if (i > 0 && data_[i - 1] != 0) continue;   // must start a string
        bool match = true;
        for (size_t k = 0; k < prefix.size() && match; ++k) match = data_[i + k] == (uint8_t)prefix[k];
        if (!match) continue;
        std::string s;
        size_t k = i;
        bool printable = true;
        while (k < data_.size() && data_[k] != 0) {
            printable = printable && data_[k] >= 0x20 && data_[k] < 0x7F;
            s += char(data_[k++]);
        }
        if (k == data_.size()) return false;
        if (printable) { out = s; return true; }
    }
    return false;
}

bool decodeDib(const std::vector<uint8_t>& dib, Image& img, std::string& error) {
    Reader r(dib);
    const uint32_t hdr = r.u32(0);
    int w, h, bpp;
    uint32_t compression = 0, used = 0;
    size_t entry;
    if (hdr == 12) {
        w = r.u16(4);
        h = int16_t(r.u16(6));
        bpp = r.u16(10);
        entry = 3;
    } else if (hdr >= 40) {
        w = int32_t(r.u32(4));
        h = int32_t(r.u32(8));
        bpp = r.u16(14);
        compression = r.u32(16);
        used = r.u32(32);
        entry = 4;
    } else {
        error = "unknown bitmap header";
        return false;
    }
    const bool topDown = h < 0;
    if (topDown) h = -h;
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192) { error = "bad bitmap size"; return false; }
    if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 24) { error = "unsupported bit depth " + number(bpp); return false; }
    std::vector<uint32_t> pal;
    if (bpp <= 8) {
        const uint32_t n = used ? used : (1u << bpp);
        if (n > 256) { error = "bad palette"; return false; }
        for (uint32_t i = 0; i < n; ++i) {
            const size_t o = hdr + i * entry;
            pal.push_back(argb(r.u8(o + 2), r.u8(o + 1), r.u8(o)));
        }
    }
    const size_t bits = hdr + pal.size() * entry;
    img.width = w;
    img.height = h;
    img.argb.assign(size_t(w) * h, 0xFF000000u);
    if (compression == 1 || compression == 2) {   // RLE8, RLE4
        const bool four = compression == 2;
        std::vector<uint8_t> idx(size_t(w) * h, 0);
        int x = 0, y = 0;
        size_t pos = bits;
        while (pos + 1 < dib.size()) {
            const uint8_t n = r.u8(pos), v = r.u8(pos + 1);
            pos += 2;
            if (n) {
                for (int i = 0; i < n; ++i, ++x)
                    if (x < w && y < h) idx[size_t(y) * w + x] = four ? ((i & 1) ? (v & 0xF) : (v >> 4)) : v;
                continue;
            }
            if (v == 0) { x = 0; ++y; continue; }
            if (v == 1) break;
            if (v == 2) { x += r.u8(pos); y += r.u8(pos + 1); pos += 2; continue; }
            const size_t bytes = four ? (v + 1u) / 2u : v;
            for (int i = 0; i < v; ++i, ++x) {
                uint8_t c = four ? r.u8(pos + i / 2) : r.u8(pos + i);
                if (four) c = (i & 1) ? (c & 0xF) : (c >> 4);
                if (x < w && y < h) idx[size_t(y) * w + x] = c;
            }
            pos += bytes + (bytes & 1);
        }
        for (int yy = 0; yy < h; ++yy)
            for (int xx = 0; xx < w; ++xx) {
                const uint8_t i = idx[size_t(yy) * w + xx];
                img.argb[size_t(h - 1 - yy) * w + xx] = i < pal.size() ? pal[i] : 0xFF000000u;
            }
        return r.ok;
    }
    if (compression != 0) { error = "unsupported bitmap compression"; return false; }
    const size_t stride = ((size_t(w) * bpp + 31) / 32) * 4;
    for (int row = 0; row < h; ++row) {
        const size_t line = bits + row * stride;
        uint32_t* out = &img.argb[size_t(topDown ? row : h - 1 - row) * w];
        for (int x = 0; x < w; ++x) {
            uint32_t i;
            switch (bpp) {
                case 1: i = (r.u8(line + x / 8) >> (7 - x % 8)) & 1; out[x] = i < pal.size() ? pal[i] : 0xFF000000u; break;
                case 4: i = (r.u8(line + x / 2) >> ((x & 1) ? 0 : 4)) & 0xF; out[x] = i < pal.size() ? pal[i] : 0xFF000000u; break;
                case 8: i = r.u8(line + x); out[x] = i < pal.size() ? pal[i] : 0xFF000000u; break;
                default: out[x] = argb(r.u8(line + x * 3 + 2), r.u8(line + x * 3 + 1), r.u8(line + x * 3)); break;
            }
        }
    }
    if (!r.ok) error = "truncated bitmap";
    return r.ok;
}

bool decodeDialog(const std::vector<uint8_t>& data, Dialog& d) {
    Reader r(data);
    size_t pos = 0;
    d.style = r.u32(0);
    const int count = r.u8(4);
    d.x = int16_t(r.u16(5)); d.y = int16_t(r.u16(7)); d.w = int16_t(r.u16(9)); d.h = int16_t(r.u16(11));
    pos = 13;
    if (r.u8(pos) == 0xFF) pos += 3; else r.cstring(pos);   // menu
    r.cstring(pos);                                         // class
    d.caption = r.cstring(pos);
    if (d.style & 0x40) { pos += 2; r.cstring(pos); }       // DS_SETFONT: size, name
    d.controls.clear();
    for (int i = 0; i < count && r.ok; ++i) {
        DialogControl c;
        c.x = int16_t(r.u16(pos)); c.y = int16_t(r.u16(pos + 2));
        c.w = int16_t(r.u16(pos + 4)); c.h = int16_t(r.u16(pos + 6));
        c.id = r.u16(pos + 8);
        c.style = r.u32(pos + 10);
        pos += 14;
        const uint8_t cls = r.u8(pos);
        if (cls & 0x80) {
            static const char* const names[] = { "button", "edit", "static", "listbox", "scrollbar", "combobox" };
            c.cls = (cls - 0x80) < 6 ? names[cls - 0x80] : "";
            ++pos;
        } else {
            c.cls = r.cstring(pos);
        }
        if (r.u8(pos) == 0xFF) { c.text = "#" + number(r.u16(pos + 1)); pos += 3; }
        else c.text = r.cstring(pos);
        pos += 1 + r.u8(pos);
        d.controls.push_back(c);
    }
    return r.ok;
}

}  // namespace x360

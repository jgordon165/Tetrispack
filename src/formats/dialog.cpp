#include "formats/dialog.h"

#include "formats/binreader.h"

namespace tp {

namespace {

std::string cstring(BinReader& r) {
    std::string s;
    for (uint8_t c; (c = r.u8()) != 0;) s.push_back(static_cast<char>(c));
    return s;
}

// A name or an ordinal: 0xFF then a 16-bit number, else a NUL-terminated string.
std::string nameOrId(BinReader& r) {
    if (r.u8At(r.tell()) == 0xFF) {
        r.u8();
        return "#" + std::to_string(r.u16());
    }
    return cstring(r);
}

const char* predefinedClass(uint8_t c) {
    switch (c) {
        case 0x80: return "button";
        case 0x81: return "edit";
        case 0x82: return "static";
        case 0x83: return "listbox";
        case 0x84: return "scrollbar";
        case 0x85: return "combobox";
        default:   return "";
    }
}

int16_t s16(BinReader& r) { return static_cast<int16_t>(r.u16()); }

}  // namespace

Dialog decodeDialog(std::span<const uint8_t> data) {
    BinReader r(data);
    Dialog d;
    d.style = r.u32();
    const int count = r.u8();
    d.x = s16(r); d.y = s16(r); d.w = s16(r); d.h = s16(r);
    d.menu = nameOrId(r);
    d.cls = cstring(r);
    d.caption = cstring(r);
    constexpr uint32_t DS_SETFONT = 0x40;
    if (d.style & DS_SETFONT) {
        d.fontSize = r.u16();
        d.fontName = cstring(r);
    }
    for (int i = 0; i < count; ++i) {
        DialogControl c;
        c.x = s16(r); c.y = s16(r); c.w = s16(r); c.h = s16(r);
        c.id = r.u16();
        c.style = r.u32();
        const uint8_t cls = r.u8At(r.tell());
        if (cls & 0x80) { r.u8(); c.cls = predefinedClass(cls); }
        else c.cls = cstring(r);
        c.text = nameOrId(r);
        r.skip(r.u8());   // creation data
        d.controls.push_back(std::move(c));
    }
    return d;
}

}  // namespace tp

#include "formats/ne.h"

#include <algorithm>
#include <cctype>

namespace tp {

const char* resTypeName(uint16_t type) {
    switch (static_cast<ResType>(type)) {
        case ResType::Cursor: return "CURSOR";
        case ResType::Bitmap: return "BITMAP";
        case ResType::Icon: return "ICON";
        case ResType::Menu: return "MENU";
        case ResType::Dialog: return "DIALOG";
        case ResType::String: return "STRING";
        case ResType::FontDir: return "FONTDIR";
        case ResType::Font: return "FONT";
        case ResType::Accelerator: return "ACCELERATOR";
        case ResType::RcData: return "RCDATA";
        case ResType::GroupCursor: return "GROUP_CURSOR";
        case ResType::GroupIcon: return "GROUP_ICON";
        case ResType::Version: return "VERSION";
    }
    return "?";
}

static std::string upper(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

NeFile::NeFile(std::vector<uint8_t> data) : data_(std::move(data)) {
    BinReader r(data_);
    if (r.size() < 0x40 || r.u16At(0) != 0x5A4D)  // "MZ"
        throw FormatError("not an MZ executable");
    const size_t ne = r.u32At(0x3C);
    if (r.u16At(ne) != 0x454E)  // "NE"
        throw FormatError("not a 16-bit Windows (NE) executable");

    const size_t resTab = ne + r.u16At(ne + 0x24);
    const size_t resNames = ne + r.u16At(ne + 0x26);
    const uint16_t winVer = r.u16At(ne + 0x3E);
    winMajor_ = static_cast<uint8_t>(winVer >> 8);
    winMinor_ = static_cast<uint8_t>(winVer & 0xFF);

    // The first entry of the resident-name table is the module name.
    {
        BinReader n(data_);
        n.seek(resNames);
        moduleName_ = n.pstring();
    }
    if (resTab != resNames) parseResources(r, resTab, resNames);
}

void NeFile::parseResources(BinReader& r, size_t tableStart, size_t tableEnd) {
    auto nameAt = [&](uint16_t rel) {
        BinReader n(data_);
        n.seek(tableStart + rel);
        return upper(n.pstring());
    };

    r.seek(tableStart);
    const uint16_t shift = r.u16();
    if (shift > 16) throw FormatError("bad resource alignment shift");
    for (;;) {
        if (r.tell() >= tableEnd) throw FormatError("resource table not terminated");
        const uint16_t type = r.u16();
        if (type == 0) break;
        const uint16_t count = r.u16();
        r.skip(4);  // reserved (loader function pointer)
        for (uint16_t i = 0; i < count; ++i) {
            NeResource res;
            if (type & 0x8000) res.typeId = type & 0x7FFF;
            else res.typeName = nameAt(type);
            res.offset = static_cast<uint32_t>(r.u16()) << shift;
            res.length = static_cast<uint32_t>(r.u16()) << shift;
            res.flags = r.u16();
            const uint16_t id = r.u16();
            r.skip(4);  // reserved (handle, usage)
            if (id & 0x8000) res.key.id = id & 0x7FFF;
            else res.key.name = nameAt(id);
            // Lengths are rounded up to the alignment: clamp to the file.
            if (res.offset > data_.size()) throw FormatError("resource past end of file");
            res.length = std::min<uint32_t>(res.length, static_cast<uint32_t>(data_.size() - res.offset));
            resources_.push_back(std::move(res));
        }
    }
}

const NeResource* NeFile::find(uint16_t type, const ResKey& key) const {
    const std::string want = upper(key.name);
    for (const auto& res : resources_) {
        if (res.typeId != type) continue;
        if (key.name.empty() ? (res.key.name.empty() && res.key.id == key.id) : res.key.name == want)
            return &res;
    }
    return nullptr;
}

const NeResource* NeFile::find(uint16_t type, const std::string& name) const {
    // "#12" selects by number, as Windows' own FindResource does.
    if (!name.empty() && name[0] == '#') {
        const unsigned long id = std::stoul(name.substr(1));
        return find(type, static_cast<uint16_t>(id));
    }
    return find(type, ResKey{0, name});
}

std::vector<uint8_t> NeFile::read(const NeResource& r) const {
    auto b = BinReader(data_).bytes(r.offset, r.length);
    return std::vector<uint8_t>(b.begin(), b.end());
}

std::optional<std::string> NeFile::string(uint16_t id) const {
    const NeResource* block = find(static_cast<uint16_t>(ResType::String), static_cast<uint16_t>(id / 16 + 1));
    if (!block) return std::nullopt;
    const auto bytes = read(*block);
    BinReader r(bytes);
    try {
        for (unsigned i = 0; i < 16; ++i) {
            std::string s = r.pstring();
            if (i == id % 16) {
                if (s.empty()) return std::nullopt;
                return s;
            }
        }
    } catch (const FormatError&) {
    }
    return std::nullopt;
}

std::optional<std::string> NeFile::cstringStartingWith(const std::string& prefix) const {
    if (prefix.empty()) return std::nullopt;
    const auto begin = data_.begin(), end = data_.end();
    for (auto it = std::search(begin, end, prefix.begin(), prefix.end()); it != end;
         it = std::search(it + 1, end, prefix.begin(), prefix.end())) {
        if (it != begin && *(it - 1) != 0) continue;   // must start a string
        const auto stop = std::find(it, end, uint8_t{0});
        if (stop == end) break;
        std::string s(it, stop);
        bool printable = true;
        for (unsigned char c : s) printable = printable && c >= 0x20 && c < 0x7F;
        if (printable) return s;
    }
    return std::nullopt;
}

}  // namespace tp

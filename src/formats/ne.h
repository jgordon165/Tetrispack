// The 16-bit Windows "New Executable" (NE) format: Windows 3.x programs such
// as the Entertainment Pack's TETRIS.EXE. We read its resource table, which
// holds the art (BITMAP), icons, menus, dialogs and string tables. See
// docs/formats.md.
#pragma once

#include "formats/binreader.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tp {

// Standard Windows resource type numbers.
enum class ResType : uint16_t {
    Cursor = 1, Bitmap = 2, Icon = 3, Menu = 4, Dialog = 5, String = 6,
    FontDir = 7, Font = 8, Accelerator = 9, RcData = 10,
    GroupCursor = 12, GroupIcon = 14, Version = 16,
};

const char* resTypeName(uint16_t type);

// A resource is named either by number (id) or by string (name, upper case).
struct ResKey {
    uint16_t id = 0;     // 0 when named
    std::string name;    // empty when numbered

    std::string str() const { return name.empty() ? "#" + std::to_string(id) : name; }
};

struct NeResource {
    uint16_t typeId = 0;      // 0 when the type is named
    std::string typeName;     // empty when numbered
    ResKey key;
    uint32_t offset = 0;      // byte offset in the file
    uint32_t length = 0;      // bytes
    uint16_t flags = 0;
};

class NeFile {
public:
    // Throws FormatError when the data isn't an NE executable.
    explicit NeFile(std::vector<uint8_t> data);

    static NeFile load(const std::string& path) { return NeFile(readFile(path)); }

    const std::string& moduleName() const { return moduleName_; }
    uint8_t expectedWindowsMajor() const { return winMajor_; }
    uint8_t expectedWindowsMinor() const { return winMinor_; }
    const std::vector<NeResource>& resources() const { return resources_; }

    const NeResource* find(uint16_t type, const ResKey& key) const;
    const NeResource* find(uint16_t type, uint16_t id) const { return find(type, ResKey{id, {}}); }
    const NeResource* find(uint16_t type, const std::string& name) const;
    std::vector<uint8_t> read(const NeResource& r) const;

    // String table lookup: Windows stores strings in blocks of 16, block
    // (id / 16) + 1, each entry a length byte then the characters.
    std::optional<std::string> string(uint16_t id) const;

    // The first NUL-terminated string anywhere in the file that starts with
    // `prefix` (text the program keeps in its data segment rather than in a
    // resource, such as the version line it hands its About box).
    std::optional<std::string> cstringStartingWith(const std::string& prefix) const;

private:
    void parseResources(BinReader& r, size_t tableStart, size_t tableEnd);

    std::vector<uint8_t> data_;
    std::string moduleName_;
    uint8_t winMajor_ = 0, winMinor_ = 0;
    std::vector<NeResource> resources_;
};

}  // namespace tp

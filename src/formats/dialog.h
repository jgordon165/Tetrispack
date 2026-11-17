// Windows 3.x DIALOG resources (DLGTEMPLATE, the 16-bit form): the dialog's
// box in dialog units, then each control's box, id, style, class and text.
// The About box's credits come from ABOUTTET.DLL's dialog this way.
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace tp {

struct DialogControl {
    int x = 0, y = 0, w = 0, h = 0;   // dialog units
    uint16_t id = 0;
    uint32_t style = 0;
    std::string cls;                   // "button", "edit", "static", ... or a registered name
    std::string text;                  // "#n" when the text is a resource id (icons, bitmaps)
};

struct Dialog {
    uint32_t style = 0;
    int x = 0, y = 0, w = 0, h = 0;
    std::string menu, cls, caption;
    int fontSize = 0;                  // DS_SETFONT only
    std::string fontName;
    std::vector<DialogControl> controls;
};

// Throws FormatError on a truncated or malformed template.
Dialog decodeDialog(std::span<const uint8_t> data);

}  // namespace tp

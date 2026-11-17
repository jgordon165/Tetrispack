// State shared by the C++ host and the Lua API.
#pragma once

#include "audio.h"
#include "formats/ne.h"
#include "video.h"

#include <optional>
#include <string>

struct Engine {
    Video video;
    Audio audio;
    std::string dataArg;          // --data: folder holding TETRIS.EXE, or the file itself
    std::string exePath;          // the TETRIS.EXE found, if any
    std::optional<tp::NeFile> exe;
    std::string dataError;        // why there's no data, for the no-data screen
    bool quit = false;
    int exitCode = 0;

    // Finds and parses TETRIS.EXE. Returns false (and sets dataError) if not found.
    bool loadData();
};

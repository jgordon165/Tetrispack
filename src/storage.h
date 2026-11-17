// Where the game keeps what it remembers between runs (the options, the
// high scores): small text files in the per-user folder SDL picks for the
// platform (e.g. %APPDATA%\jgordon165\Tetrispack, ~/.local/share/...,
// ~/Library/Application Support/...). Named by the game with fixed names
// only (letters, digits, '_'). Headless runs (the tests) keep them in
// memory, so they never touch a player's files.
#pragma once

#include <map>
#include <string>

class Storage {
public:
    // persistent = false: in memory only.
    void open(bool persistent);
    bool read(const std::string& name, std::string& out) const;
    bool write(const std::string& name, const std::string& text);
    static bool validName(const std::string& name);

private:
    std::string dir_;                         // with a trailing separator; empty: memory
    std::map<std::string, std::string> mem_;
};

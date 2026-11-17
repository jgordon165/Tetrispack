#include "storage.h"

#include <SDL.h>

#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

void Storage::open(bool persistent) {
    dir_.clear();
    if (!persistent) return;
    if (char* p = SDL_GetPrefPath("jgordon165", "Tetrispack")) {
        dir_ = p;
        SDL_free(p);
    }
}

bool Storage::validName(const std::string& name) {
    if (name.empty() || name.size() > 32) return false;
    for (char c : name)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    return true;
}

bool Storage::read(const std::string& name, std::string& out) const {
    if (!validName(name)) return false;
    if (dir_.empty()) {
        const auto it = mem_.find(name);
        if (it == mem_.end()) return false;
        out = it->second;
        return true;
    }
    FILE* f = std::fopen((dir_ + name + ".txt").c_str(), "rb");
    if (!f) return false;
    out.clear();
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    std::fclose(f);
    return true;
}

// Writes name.tmp (mode 0644), then renames it over name.txt, so a crash
// mid-write leaves the old file.
bool Storage::write(const std::string& name, const std::string& text) {
    if (!validName(name)) return false;
    if (dir_.empty()) {
        mem_[name] = text;
        return true;
    }
    const std::string tmp = dir_ + name + ".tmp", path = dir_ + name + ".txt";
#ifdef _WIN32
    const int fd = _open(tmp.c_str(), _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, _S_IREAD | _S_IWRITE);
    FILE* f = fd < 0 ? nullptr : _fdopen(fd, "wb");
#else
    const int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    FILE* f = fd < 0 ? nullptr : fdopen(fd, "wb");
#endif
    if (!f) return false;
    const bool ok = text.empty() || std::fwrite(text.data(), 1, text.size(), f) == text.size();
    if (std::fclose(f) != 0 || !ok) return false;
#ifdef _WIN32
    std::remove(path.c_str());   // rename won't replace on Windows
#endif
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

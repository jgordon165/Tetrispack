// Runs the Xbox 360 host on a PC with no window or sound: the same Game
// (Lua scripts, canvas, original's files) the console runs, for tests in CI.
//
//   tetrispack360_headless --root <dir> [--data <dir>] [--scripts <dir>]
//       [--frames N] [--screenshot out.bmp] [--keys 120:pad start,300:pad a]
//
// Folders are relative to the root and end in '/'; the defaults match the
// console's layout (data/, scripts/, music/, sfx/). Keys are pressed at the
// given frames, by the names the console sends ("pad start", "pad dpleft").
// Exits non-zero on a Lua error.
#include "host.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

namespace {

class Headless : public x360::Platform {
public:
    explicit Headless(const std::string& root) : root_(root), ms_(0) {}
    bool readFile(const std::string& path, std::vector<uint8_t>& out) {
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) return false;
        out.clear();
        unsigned char buf[65536];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.insert(out.end(), buf, buf + n);
        std::fclose(f);
        return true;
    }
    bool fileExists(const std::string& path) {
        FILE* f = std::fopen(path.c_str(), "rb");
        if (f) std::fclose(f);
        return f != 0;
    }
    char separator() { return '/'; }
    std::string root() { return root_; }
    const char* name() { return "Xbox 360 (headless)"; }
    uint32_t ticks() { return ms_; }
    void log(const std::string& s) { std::printf("%s\n", s.c_str()); }
    bool hasAudio() { return false; }
    void playMusic(const std::string& path, double) { std::printf("music: %s\n", path.c_str()); }
    void stopMusic(double) {}
    void pauseMusic(bool) {}
    void setMusicVolume(float) {}
    double musicPosition() { return 0; }
    void playSfx(const std::string& name, float) { ++sfx[name]; }
    void setSfxVolume(float) {}

    void advance(uint32_t ms) { ms_ += ms; }
    std::map<std::string, int> sfx;

private:
    std::string root_;
    uint32_t ms_;
};

}  // namespace

int main(int argc, char** argv) {
    std::string root = "./", data = "data/", scripts = "scripts/", shot, keys;
    long frames = 120;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const char* v = i + 1 < argc ? argv[i + 1] : "";
        if (a == "--root") { root = v; ++i; }
        else if (a == "--data") { data = v; ++i; }
        else if (a == "--scripts") { scripts = v; ++i; }
        else if (a == "--frames") { frames = std::atol(v); ++i; }
        else if (a == "--screenshot") { shot = v; ++i; }
        else if (a == "--keys") { keys = v; ++i; }
        else { std::fprintf(stderr, "unknown option %s\n", a.c_str()); return 2; }
    }
    if (!root.empty() && root[root.size() - 1] != '/') root += '/';

    // "120:pad start,300:pad a"
    std::multimap<long, std::string> presses;
    for (size_t p = 0; p < keys.size();) {
        size_t comma = keys.find(',', p);
        if (comma == std::string::npos) comma = keys.size();
        const std::string item = keys.substr(p, comma - p);
        const size_t colon = item.find(':');
        if (colon != std::string::npos) presses.insert(std::make_pair(std::atol(item.c_str()), item.substr(colon + 1)));
        p = comma + 1;
    }

    Headless platform(root);
    x360::Game game(platform);
    if (!game.boot(data, scripts)) {
        std::fprintf(stderr, "lua: %s\n", game.error().c_str());
        return 1;
    }
    for (long f = 1; f <= frames && !game.quitRequested(); ++f) {
        std::pair<std::multimap<long, std::string>::iterator, std::multimap<long, std::string>::iterator> r =
            presses.equal_range(f);
        for (std::multimap<long, std::string>::iterator it = r.first; it != r.second; ++it)
            if (!game.key(it->second)) { std::fprintf(stderr, "lua: %s\n", game.error().c_str()); return 1; }
        platform.advance(16);
        if (!game.frame(1.0 / 60.0)) {
            std::fprintf(stderr, "lua: %s\n", game.error().c_str());
            return 1;
        }
    }
    for (std::map<std::string, int>::iterator it = platform.sfx.begin(); it != platform.sfx.end(); ++it)
        std::printf("sfx %s: %d\n", it->first.c_str(), it->second);
    if (!shot.empty() && !game.canvas().saveBmp(shot)) {
        std::fprintf(stderr, "can't write %s\n", shot.c_str());
        return 1;
    }
    return 0;
}

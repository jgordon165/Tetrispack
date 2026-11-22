// Runs the Xbox 360 host on a PC with no window or sound: the same Game
// (Lua scripts, canvas, original's files) the console runs, for tests in CI.
//
//   tetrispack360_headless [--frames N] [--screenshot] [--keys 120:pad start,300:pad a]
//
// Run it from a folder laid out like the console's content folder (data/,
// scripts/, music/, sfx/; the repository's root works). The sound is mixed
// as on the console (mixer360.cpp), at 48 kHz, and measured, not played:
// "sound rms" at the end is its loudness over the run. Keys are pressed at
// the given frames, by the names the console sends ("pad start",
// "pad dpleft"). --screenshot saves the last frame as tetrispack360.bmp in
// that folder. Exits non-zero on a Lua error.
#include "host.h"
#include "exit360.h"
#include "mixer360.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

// A file in the current folder, created with mode 0644.
bool writeFile(const char* name, const std::vector<uint8_t>& data) {
#ifdef _WIN32
    const int fd = _open(name, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, _S_IREAD | _S_IWRITE);
    FILE* f = fd < 0 ? 0 : _fdopen(fd, "wb");
#else
    const int fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    FILE* f = fd < 0 ? 0 : fdopen(fd, "wb");
#endif
    if (!f) return false;
    const bool ok = data.empty() || std::fwrite(&data[0], 1, data.size(), f) == data.size();
    return std::fclose(f) == 0 && ok;
}

class Headless : public x360::Platform {
public:
    Headless() : ms_(0), audio_(*this, kRate), sum2_(0), mixed_(0) {}
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
    std::string root() { return ""; }   // the current folder
    const char* name() { return "Xbox 360 (headless)"; }
    // tetrispack.ini, as on the console (here as if run from Usb0:\tetrispack).
    std::string exitName() { return exit().name; }
    x360::ExitTarget exit() {
        std::vector<uint8_t> f;
        if (!readFile("tetrispack.ini", f)) return x360::ExitTarget();
        return x360::parseExitConfig(std::string(f.begin(), f.end()), "\\Device\\Mass0\\tetrispack\\tetrispack.xex");
    }
    uint32_t ticks() { return ms_; }
    void log(const std::string& s) { std::printf("%s\n", s.c_str()); }
    bool hasAudio() { return true; }
    void playMusic(const std::string& path, double fade) {
        std::printf("music: %s\n", path.c_str());
        audio_.playMusic(path, fade);
    }
    void stopMusic(double fade) { audio_.stopMusic(fade); }
    void pauseMusic(bool on) { audio_.pauseMusic(on); }
    void setMusicVolume(float v) { audio_.setMusicVolume(v); }
    double musicPosition() { return audio_.musicPosition(); }
    // Counted for the tests; mixed too where sfx/ has them.
    void playSfx(const std::string& name, float gain) {
        ++sfx[name];
        if (fileExists(root() + "sfx/" + name + ".wav")) audio_.playSfx(root() + "sfx/" + name + ".wav", gain);
    }
    void setSfxVolume(float v) { audio_.setSfxVolume(v); }

    // One frame of 1/60 s: the time, and the sound mixed and measured.
    void advance() {
        ms_ += 16;
        audio_.tick();
        static int16_t out[kRate / 60 * 2];
        audio_.mix(out, kRate / 60);
        for (int i = 0; i < kRate / 60 * 2; ++i) sum2_ += double(out[i]) * out[i];
        mixed_ += kRate / 60 * 2;
    }
    double rms() const { return mixed_ ? std::sqrt(sum2_ / mixed_) : 0; }
    std::map<std::string, int> sfx;

private:
    enum { kRate = 48000 };
    uint32_t ms_;
    x360::Audio audio_;
    double sum2_;
    long mixed_;
};

}  // namespace

int main(int argc, char** argv) {
    std::string keys;
    bool shot = false;
    long frames = 120;
    int i = 1;
    while (i < argc) {
        const std::string a = argv[i++];
        if (a == "--screenshot") { shot = true; continue; }
        if ((a != "--frames" && a != "--keys") || i >= argc) {
            std::fprintf(stderr, "unknown option or missing value: %s\n", a.c_str());
            return 2;
        }
        const char* v = argv[i++];
        if (a == "--frames") frames = std::atol(v);
        else keys = v;
    }

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

    Headless platform;
    x360::Game game(platform);
    if (!game.boot("data/", "scripts/")) {
        std::fprintf(stderr, "lua: %s\n", game.error().c_str());
        return 1;
    }
    long f = 1;
    for (; f <= frames && !game.quitRequested(); ++f) {
        std::pair<std::multimap<long, std::string>::iterator, std::multimap<long, std::string>::iterator> r =
            presses.equal_range(f);
        for (std::multimap<long, std::string>::iterator it = r.first; it != r.second; ++it)
            if (!game.key(it->second)) { std::fprintf(stderr, "lua: %s\n", game.error().c_str()); return 1; }
        platform.advance();
        if (!game.frame(1.0 / 60.0)) {
            std::fprintf(stderr, "lua: %s\n", game.error().c_str());
            return 1;
        }
    }
    // On the console the title would end here, back to the dashboard.
    if (game.quitRequested()) {
        const x360::ExitTarget t = platform.exit();
        std::printf("quit at frame %ld, to %s\n", f, t.path.empty() ? "the dashboard" : t.path.c_str());
    }
    for (std::map<std::string, int>::iterator it = platform.sfx.begin(); it != platform.sfx.end(); ++it)
        std::printf("sfx %s: %d\n", it->first.c_str(), it->second);
    std::printf("sound rms: %d\n", int(platform.rms()));
    if (shot && !writeFile("tetrispack360.bmp", game.canvas().bmp())) {
        std::perror("tetrispack360.bmp");
        return 1;
    }
    return 0;
}

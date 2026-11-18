// The Xbox 360 platform layer on xsdk (openpak/x360-sdk), as Meguarace
// builds it: a normal title, tetrispack.xex, launched from the dashboard,
// Aurora or a file manager on a console that runs unsigned code. No
// Microsoft XDK; it builds on GitHub's Linux runners (.github/workflows,
// job "xbox360").
//
// The folder the .xex is in (game:/):
//   data/      your TETRIS.EXE (and ABOUTTET.DLL), copied there by you
//   scripts/   the game (the repository's scripts/)
//   music/     <style>_<n>.wav, rendered ahead of time (16-bit PCM)
//   sfx/       <name>.wav
//
// The picture: the canvas scaled into one of xsdk's three 1280x720 front
// buffers, in the tiled layout the display reads (each pixel stored B, G, R,
// A), flipped at vblank. The pad: XAM's input state, as the game's "pad ..."
// names. The sound: the kernel's render driver asks for 256 frames at a
// time, six channels of floats; a feeder thread mixes the music and effects
// (mixer360.cpp) into a ring it reads from.
#include "host.h"
#include "mixer360.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <sys/stat.h>
#include <sys/times.h>
#include <ctime>

#include "xsdk/display.h"
#include "xsdk/gpu.h"
#include "xsdk/kernel.h"
#include "xsdk/log.h"
#include "xsdk/present.h"
#include "xsdk/thread.h"

// In libxsdk.a (its private gpu_private.h): the byte offset of texel (x, y)
// in Xenos's tiled layout.
extern "C" uint32_t xsdk_tiled_2d(uint32_t x, uint32_t y, uint32_t pitch_texels, uint32_t bytes_per_texel_log2);

namespace {

const uint64_t kTimeBase = 49875000ull;   // xsdk src/time.c: the CPU's 3192 MHz over 64

uint64_t timeBase() {
    uint32_t upper, lower, check;
    do {
        __asm__ volatile("mftbu %0" : "=r"(upper));
        __asm__ volatile("mftb %0" : "=r"(lower));
        __asm__ volatile("mftbu %0" : "=r"(check));
    } while (upper != check);
    return (uint64_t(upper) << 32) | lower;
}

bool readWhole(const std::string& path, std::vector<uint8_t>& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    out.clear();
    static unsigned char buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.insert(out.end(), buf, buf + n);
    std::fclose(f);
    return true;
}

// ---- the picture --------------------------------------------------------------

class Screen {
public:
    Screen() : up_(false), tvW_(1280), tvH_(720), words_(0), cw_(0), ch_(0) {}

    bool init() {
        uint32_t w = 0, h = 0;
        if (!NT_SUCCESS(xsdk_gpu_open())) xsdk_log_write("gpu open failed");
        // Three buffers, each frame shown for one vblank at least.
        if (!NT_SUCCESS(xsdk_present_open(XSDK_DISPLAY_720P, 3, 1, &w, &h)) || !w || !h) {
            xsdk_log_write("display open failed");
            return false;
        }
        tvW_ = w;
        tvH_ = h;
        words_ = xsdk_gpu_resolve_bytes(tvW_, tvH_) / 4;
        staging_.assign(words_, 0);
        tile_.resize(size_t(tvW_) * tvH_);
        for (uint32_t y = 0; y < tvH_; ++y)
            for (uint32_t x = 0; x < tvW_; ++x) tile_[y * tvW_ + x] = xsdk_tiled_2d(x, y, tvW_, 2) >> 2;
        xsdk_log_write("display %ux%u", unsigned(tvW_), unsigned(tvH_));
        up_ = true;
        return true;
    }

    // The canvas, scaled to the TV's height with its aspect kept (nearest
    // neighbour), black at the sides.
    void present(const x360::Canvas& c) {
        if (!up_ || !c.pixels()) return;
        if (c.width() != cw_ || c.height() != ch_) layout(c.width(), c.height());
        const uint32_t* src = c.pixels();
        for (size_t y = 0; y < rowSrc_.size(); ++y) {
            const uint32_t* line = src + size_t(rowSrc_[y]) * cw_;
            const uint32_t* idx = &tile_[(oy_ + y) * tvW_ + ox_];
            for (size_t x = 0; x < colSrc_.size(); ++x)
                staging_[idx[x]] = __builtin_bswap32(line[colSrc_[x]] | 0xFF000000u);
        }
        uint32_t address = 0;
        if (!NT_SUCCESS(xsdk_present_begin(&address, 200))) return;
        // Write-combined memory: one pass of 32-bit stores, in order.
        volatile uint32_t* dst = xsdk_present_pixels(address);
        for (uint32_t i = 0; i < words_; ++i) dst[i] = staging_[i];
        __asm__ volatile("sync" ::: "memory");
        xsdk_present_end();
    }

private:
    void layout(int w, int h) {
        cw_ = w;
        ch_ = h;
        uint32_t dw = tvW_, dh = uint32_t(uint64_t(tvW_) * h / w);
        if (dh > tvH_) {
            dh = tvH_;
            dw = uint32_t(uint64_t(tvH_) * w / h);
        }
        ox_ = (tvW_ - dw) / 2;
        oy_ = (tvH_ - dh) / 2;
        colSrc_.resize(dw);
        rowSrc_.resize(dh);
        for (uint32_t x = 0; x < dw; ++x) colSrc_[x] = int(uint64_t(x) * w / dw);
        for (uint32_t y = 0; y < dh; ++y) rowSrc_[y] = int(uint64_t(y) * h / dh);
        const uint32_t black = __builtin_bswap32(0xFF000000u);
        for (size_t i = 0; i < staging_.size(); ++i) staging_[i] = black;
    }

    bool up_;
    uint32_t tvW_, tvH_, words_, ox_, oy_;
    int cw_, ch_;
    std::vector<uint32_t> staging_, tile_;
    std::vector<int> colSrc_, rowSrc_;
};

// ---- the sound ------------------------------------------------------------------

const int kOutRate = 48000;
enum { kDriverFrames = 256, kDriverChannels = 6, kRingFrames = 16384, kFillTo = 4800 };   // 0.1 s ahead

int16_t ring[kRingFrames * 2];
uint32_t ringRead, ringWrite;   // in frames, counting up
float* driverFrame;
uint32_t driver;

// The kernel's render driver, at interrupt time: the ring, or silence.
extern "C" void tetrispack_audio_callback(const uint32_t* context) {
    (void)context;
    const uint32_t d = __atomic_load_n(&driver, __ATOMIC_ACQUIRE);
    uint32_t r = __atomic_load_n(&ringRead, __ATOMIC_RELAXED);
    const uint32_t w = __atomic_load_n(&ringWrite, __ATOMIC_ACQUIRE);
    if (!d) return;
    for (int i = 0; i < kDriverFrames; ++i) {
        float l = 0, rr = 0;
        if (r != w) {
            const int16_t* s = &ring[(r % kRingFrames) * 2];
            l = float(s[0]) / 32768.0f;
            rr = float(s[1]) / 32768.0f;
            ++r;
        }
        driverFrame[0 * kDriverFrames + i] = l;    // front left
        driverFrame[1 * kDriverFrames + i] = rr;   // front right
        for (int c = 2; c < kDriverChannels; ++c) driverFrame[c * kDriverFrames + i] = 0;
    }
    __atomic_store_n(&ringRead, r, __ATOMIC_RELEASE);
    XAudioSubmitRenderDriverFrame(d, driverFrame);
}

// The mixer's lock, for the feeder thread and the game's.
struct CriticalSection : x360::Audio::Lock {
    CriticalSection() { RtlInitializeCriticalSection(&cs); }
    void lock() { RtlEnterCriticalSection(&cs); }
    void unlock() { RtlLeaveCriticalSection(&cs); }
    RTL_CRITICAL_SECTION cs;
};

class Xbox : public x360::Platform {
public:
    Xbox() : audio_(*this, kOutRate, &lock_), feeder_(0), running_(false) {}

    bool initAudio() {
        driverFrame = (float*)MmAllocatePhysicalMemoryEx(0, kDriverFrames * kDriverChannels * sizeof(float),
                                                         PAGE_READWRITE, 0, 0xFFFFFFFFu, 0);
        if (!driverFrame) { xsdk_log_write("audio: no memory"); return false; }
        std::memset(driverFrame, 0, kDriverFrames * kDriverChannels * sizeof(float));
        XAUDIO_RENDER_DRIVER_CLIENT client;
        client.callback = tetrispack_audio_callback;
        client.context = 0;
        uint32_t registered = 0;
        if (XAudioRegisterRenderDriverClient(&client, &registered) != ERROR_SUCCESS) {
            xsdk_log_write("audio: XAudioRegisterRenderDriverClient failed");
            return false;
        }
        __atomic_store_n(&driver, registered, __ATOMIC_RELEASE);
        // The feeder runs on hardware thread 2 (the game on 0), so a slow
        // frame doesn't starve the sound.
        running_ = true;
        if (!NT_SUCCESS(xsdk_thread_create_on(&feeder_, 256 * 1024, feederMain, this, 2))) {
            xsdk_log_write("audio: no feeder thread; mixing between frames");
            feeder_ = 0;
        }
        return true;
    }

    // Once a frame: a waiting song starts once the last has faded out.
    void tick() {
        if (!feeder_) fill();
        audio_.tick();
    }

    // Platform
    bool readFile(const std::string& path, std::vector<uint8_t>& out) { return readWhole(path, out); }
    bool fileExists(const std::string& path) {
        struct stat st;
        return stat(path.c_str(), &st) == 0;
    }
    char separator() { return '/'; }
    std::string root() { return "game:/"; }
    const char* name() { return "Xbox 360"; }
    uint32_t ticks() { return uint32_t(timeBase() / (kTimeBase / 1000)); }
    void log(const std::string& s) { xsdk_log_write("%s", s.c_str()); }
    bool hasAudio() { return __atomic_load_n(&driver, __ATOMIC_ACQUIRE) != 0; }
    void playMusic(const std::string& path, double fade) { if (hasAudio()) audio_.playMusic(path, fade); }
    void stopMusic(double fade) { audio_.stopMusic(fade); }
    void pauseMusic(bool on) { audio_.pauseMusic(on); }
    void setMusicVolume(float v) { audio_.setMusicVolume(v); }
    double musicPosition() { return audio_.musicPosition(); }
    void playSfx(const std::string& name, float gain) {
        if (hasAudio()) audio_.playSfx(root() + "sfx/" + name + ".wav", gain);
    }
    void setSfxVolume(float v) { audio_.setSfxVolume(v); }

private:
    // Tops the ring up to kFillTo frames ahead of the driver.
    void fill() {
        int16_t chunk[512 * 2];
        for (;;) {
            const uint32_t w = __atomic_load_n(&ringWrite, __ATOMIC_RELAXED);
            const uint32_t r = __atomic_load_n(&ringRead, __ATOMIC_ACQUIRE);
            const uint32_t queued = w - r;
            if (queued >= kFillTo) return;
            uint32_t n = kFillTo - queued;
            if (n > 512) n = 512;
            audio_.mix(chunk, int(n));
            for (uint32_t i = 0; i < n; ++i) {
                int16_t* d = &ring[((w + i) % kRingFrames) * 2];
                d[0] = chunk[2 * i];
                d[1] = chunk[2 * i + 1];
            }
            __atomic_store_n(&ringWrite, w + n, __ATOMIC_RELEASE);
        }
    }

    static uint32_t feederMain(void* self) {
        Xbox* x = static_cast<Xbox*>(self);
        while (x->running_) {
            x->fill();
            xsdk_thread_sleep(2);
        }
        return 0;
    }

    CriticalSection lock_;
    x360::Audio audio_;
    HANDLE feeder_;
    volatile bool running_;
};

// ---- the pad ------------------------------------------------------------------

// The pad's buttons as the game's input names (SDL's controller names, as on
// the PC: scripts/ui/input.lua). Left / Right on the D-pad or the left stick
// repeat while held, like a keyboard (Down drops, so it doesn't).
struct Button { uint16_t mask; const char* name; bool repeats; };
const Button kButtons[] = {
    { INPUT_GAMEPAD_DPAD_UP, "pad dpup", false }, { INPUT_GAMEPAD_DPAD_DOWN, "pad dpdown", false },
    { INPUT_GAMEPAD_DPAD_LEFT, "pad dpleft", true }, { INPUT_GAMEPAD_DPAD_RIGHT, "pad dpright", true },
    { INPUT_GAMEPAD_START, "pad start", false }, { INPUT_GAMEPAD_BACK, "pad back", false },
    { INPUT_GAMEPAD_LEFT_SHOULDER, "pad leftshoulder", false },
    { INPUT_GAMEPAD_RIGHT_SHOULDER, "pad rightshoulder", false },
    { INPUT_GAMEPAD_A, "pad a", false }, { INPUT_GAMEPAD_B, "pad b", false },
    { INPUT_GAMEPAD_X, "pad x", false }, { INPUT_GAMEPAD_Y, "pad y", false },
};
const int kButtonCount = sizeof(kButtons) / sizeof(kButtons[0]);
const uint32_t kRepeatDelay = 250, kRepeatRate = 50;   // ms

// Every connected pad's buttons together, the left stick as a D-pad.
uint16_t padButtons() {
    uint16_t b = 0;
    for (uint32_t port = 0; port < 4; ++port) {
        INPUT_STATE s;
        std::memset(&s, 0, sizeof s);
        if (XamInputGetState(port, 0, &s) != ERROR_SUCCESS) continue;
        b |= s.gamepad.buttons;
        const int16_t dead = 16000;
        if (s.gamepad.thumb_lx < -dead) b |= INPUT_GAMEPAD_DPAD_LEFT;
        if (s.gamepad.thumb_lx > dead) b |= INPUT_GAMEPAD_DPAD_RIGHT;
        if (s.gamepad.thumb_ly > dead) b |= INPUT_GAMEPAD_DPAD_UP;
        if (s.gamepad.thumb_ly < -dead) b |= INPUT_GAMEPAD_DPAD_DOWN;
    }
    return b;
}

// Shows a Lua error instead of the game, so a broken scripts/ folder says why.
void showError(Screen& screen, x360::Canvas& c, const std::string& msg) {
    xsdk_log_write("stopped: %s", msg.c_str());
    c.resize(640, 440);
    c.clear(0, 0, 128);
    c.text(16, 16, "Tetrispack stopped:", 1, 255, 255, 255, x360::Canvas::System);
    int y = 40;
    for (size_t p = 0; p < msg.size() && y < 420; p += 70, y += 16)
        c.text(16, y, msg.substr(p, 70), 1, 255, 255, 0, x360::Canvas::System);
    for (;;) screen.present(c);
}

}  // namespace

// What newlib asks of the system and xsdk leaves out: clock() wants
// times(), here the time base counted as processor time.
extern "C" clock_t times(struct tms* t) {
    const clock_t c = clock_t(timeBase() / (kTimeBase / CLOCKS_PER_SEC));
    if (t) {
        t->tms_utime = c;
        t->tms_stime = 0;
        t->tms_cutime = 0;
        t->tms_cstime = 0;
    }
    return c;
}

int main(int, char**) {
    // xsdk's start-up has opened the log already (game:/xsdk_test.log or the debugger).
    xsdk_log_write("Tetrispack for the Xbox 360 (xsdk)");
    Screen screen;
    if (!screen.init()) return 1;
    Xbox platform;
    platform.initAudio();
    x360::Game game(platform);
    if (!game.boot("data/", "scripts/")) showError(screen, game.canvas(), game.error());

    uint16_t held = 0;
    uint32_t since[kButtonCount] = { 0 }, lastRepeat[kButtonCount] = { 0 };
    uint64_t last = timeBase();
    while (!game.quitRequested()) {
        const uint16_t b = padButtons();
        const uint32_t t = platform.ticks();
        for (int i = 0; i < kButtonCount; ++i) {
            const Button& k = kButtons[i];
            const bool down = (b & k.mask) != 0, was = (held & k.mask) != 0;
            bool fire = false;
            if (down && !was) {
                fire = true;
                since[i] = lastRepeat[i] = t;
            } else if (down && k.repeats && t - since[i] >= kRepeatDelay && t - lastRepeat[i] >= kRepeatRate) {
                fire = true;
                lastRepeat[i] = t;
            }
            if (fire && !game.key(k.name)) showError(screen, game.canvas(), game.error());
        }
        held = b;
        const uint64_t now = timeBase();
        double dt = double(now - last) / double(kTimeBase);
        last = now;
        if (dt > 0.1) dt = 0.1;
        if (!game.frame(dt)) showError(screen, game.canvas(), game.error());
        platform.tick();
        screen.present(game.canvas());   // waits for a front buffer: 60 frames a second
    }
    return 0;
}

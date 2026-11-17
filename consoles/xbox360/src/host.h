// The Xbox 360 host: runs the same Lua game as the PC (scripts/), with the
// original's art read from the player's TETRIS.EXE, on a small C++03 host
// the XDK's Visual C++ 2010 compiler can build. The PC engine (src/) needs
// C++20 and SDL; this host keeps to what the console has.
//
// Game is portable: it holds the Lua state, the canvas and the `tp` API.
// A Platform supplies files, time, audio and logging: platform_x360.cpp on
// the console (Direct3D 9, XInput, XAudio2), platform_headless.cpp for the
// tests that run this host on a PC.
#pragma once

#include "canvas.h"
#include "formats360.h"

#include <stdint.h>
#include <string>
#include <vector>

struct lua_State;

namespace x360 {

class Platform {
public:
    virtual ~Platform() {}
    virtual bool readFile(const std::string& path, std::vector<uint8_t>& out) = 0;
    virtual bool fileExists(const std::string& path) = 0;
    virtual char separator() = 0;                 // '\\' on the console
    virtual std::string root() = 0;               // "game:\\" on the console
    virtual const char* name() = 0;               // tp.platform()
    virtual uint32_t ticks() = 0;                 // milliseconds
    virtual void log(const std::string& s) = 0;

    // Music: songs rendered ahead of time (music\<style>_<n>.wav), looping.
    virtual bool hasAudio() = 0;
    virtual void playMusic(const std::string& path, double fade) = 0;
    virtual void stopMusic(double fade) = 0;
    virtual void pauseMusic(bool on) = 0;
    virtual void setMusicVolume(float v) = 0;
    virtual double musicPosition() = 0;
    // Sound effects: sfx\<name>.wav.
    virtual void playSfx(const std::string& name, float gain) = 0;
    virtual void setSfxVolume(float v) = 0;
};

class Game {
public:
    explicit Game(Platform& p);
    ~Game();

    // Loads the original's files from `data` (TETRIS.EXE, ABOUTTET.DLL; the
    // game also runs without them) and starts scripts/main.lua from
    // `scripts`. Folders relative to the platform's root, ending in its
    // separator. False on a Lua error (error() says what).
    bool boot(const std::string& data, const std::string& scripts);
    bool frame(double dt);                       // update, then draw
    bool key(const std::string& name);           // "pad a", "pad start", ... (ui/input.lua)
    const std::string& error() const { return error_; }
    bool quitRequested() const { return quit_; }
    Canvas& canvas() { return canvas_; }

    // Used by the Lua API.
    Platform& platform() { return p_; }
    NeFile& module(const std::string& which) { return which == "about" ? about_ : exe_; }
    const std::string& dataError() const { return dataError_; }
    const std::string& scriptsDir() const { return scripts_; }
    void requestQuit() { quit_ = true; }

private:
    bool call(const char* fn, int nargs);
    bool loadModule(NeFile& into, const std::string& dir, const char* name);

    Platform& p_;
    lua_State* L_;
    Canvas canvas_;
    NeFile exe_, about_;
    std::string scripts_, dataError_, error_;
    bool quit_;
};

}  // namespace x360

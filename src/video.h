// The window and everything drawn to it. The Lua side draws on a logical
// canvas (the original's playfield size by default); SDL scales it to the
// window with letterboxing.
#pragma once

#include "formats/dib.h"

#include <SDL.h>
#include <string>
#include <vector>

class Video {
public:
    ~Video();
    bool open(const std::string& title, int canvasW, int canvasH, double scale);
    void setTitle(const std::string& t);
    void setCanvas(int w, int h);
    int canvasW() const { return cw_; }
    int canvasH() const { return ch_; }

    void clear(uint8_t r, uint8_t g, uint8_t b);
    void fill(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    // Built-in fonts, so the port can show text without the original's:
    // Small, 3x5 (digits, A-Z, a little punctuation); System, proportional
    // and bold in the style of the Windows 3.x System font (font_system.h).
    enum class Font { Small, System };
    void text(int x, int y, const std::string& s, int scale, uint8_t r, uint8_t g, uint8_t b, Font font = Font::Small);
    static int textWidth(const std::string& s, int scale, Font font = Font::Small);

    // Uploads a decoded bitmap; returns a handle for draw(), or -1.
    int addImage(const tp::Image& img);
    void draw(int handle, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh);

    void compose();   // canvas -> window, before screenshot() and present()
    void present();
    // Saves what's in the window (the canvas as scaled to it) as a .bmp.
    bool screenshot(const std::string& path);

private:
    SDL_Window* win_ = nullptr;
    SDL_Renderer* ren_ = nullptr;
    std::vector<SDL_Texture*> tex_;
    SDL_Texture* canvas_ = nullptr;  // everything is drawn here at canvas size, then scaled to the window
    int cw_ = 0, ch_ = 0;
};

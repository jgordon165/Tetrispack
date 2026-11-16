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
    bool open(const std::string& title, int canvasW, int canvasH, int scale);
    void setTitle(const std::string& t);
    void setCanvas(int w, int h);
    int canvasW() const { return cw_; }
    int canvasH() const { return ch_; }

    void clear(uint8_t r, uint8_t g, uint8_t b);
    void fill(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    // The built-in 3x5 font (digits, A-Z and a little punctuation), so the
    // port can show text without any of the original's fonts.
    void text(int x, int y, const std::string& s, int scale, uint8_t r, uint8_t g, uint8_t b);
    static int textWidth(const std::string& s, int scale) { return s.empty() ? 0 : static_cast<int>(s.size()) * 4 * scale - scale; }

    // Uploads a decoded bitmap; returns a handle for draw(), or -1.
    int addImage(const tp::Image& img);
    void draw(int handle, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh);

    void present();

private:
    SDL_Window* win_ = nullptr;
    SDL_Renderer* ren_ = nullptr;
    std::vector<SDL_Texture*> tex_;
    int cw_ = 0, ch_ = 0;
};

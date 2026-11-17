// The Xbox 360 platform layer (XDK): Direct3D 9 shows the game's canvas,
// XInput reads the pad, XAudio2 plays the music and sound effects.
//
// The content folder next to default.xex (game:\):
//   data\      your TETRIS.EXE (and ABOUTTET.DLL), copied there by you
//   scripts\   the game (the repository's scripts/)
//   music\     <style>_<n>.wav, rendered ahead of time (16-bit PCM)
//   sfx\       <name>.wav
//
// Built only on a machine with the XDK (consoles/xbox360/Tetrispack360.vcxproj);
// everything else in this folder also builds and runs on a PC
// (platform_headless.cpp).
#include "host.h"

#include <xtl.h>
#include <xaudio2.h>
#include <d3dx9.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

const int kScreenW = 1280, kScreenH = 720;

// ---- files ------------------------------------------------------------------

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

// A RIFF WAVE file of 16-bit PCM, as XAudio2 on the 360 wants it: the
// samples byte-swapped to big-endian.
struct Wave {
    WAVEFORMATEX fmt;
    std::vector<uint8_t> data;
    Wave() { ZeroMemory(&fmt, sizeof fmt); }
};

uint32_t le32(const uint8_t* p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); }
uint16_t le16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }

bool loadWave(const std::string& path, Wave& w) {
    std::vector<uint8_t> f;
    if (!readWhole(path, f) || f.size() < 12 || memcmp(&f[0], "RIFF", 4) || memcmp(&f[8], "WAVE", 4)) return false;
    bool haveFmt = false;
    for (size_t p = 12; p + 8 <= f.size();) {
        const uint32_t len = le32(&f[p + 4]);
        const size_t body = p + 8;
        if (body + len > f.size()) break;
        if (!memcmp(&f[p], "fmt ", 4) && len >= 16) {
            w.fmt.wFormatTag = WAVE_FORMAT_PCM;
            w.fmt.nChannels = le16(&f[body + 2]);
            w.fmt.nSamplesPerSec = le32(&f[body + 4]);
            w.fmt.wBitsPerSample = le16(&f[body + 14]);
            w.fmt.nBlockAlign = WORD(w.fmt.nChannels * w.fmt.wBitsPerSample / 8);
            w.fmt.nAvgBytesPerSec = w.fmt.nSamplesPerSec * w.fmt.nBlockAlign;
            haveFmt = le16(&f[body]) == 1 && w.fmt.wBitsPerSample == 16;
        } else if (!memcmp(&f[p], "data", 4)) {
            w.data.assign(f.begin() + body, f.begin() + body + (len & ~1u));
        }
        p = body + len + (len & 1);
    }
    for (size_t i = 0; i + 1 < w.data.size(); i += 2) std::swap(w.data[i], w.data[i + 1]);
    return haveFmt && !w.data.empty();
}

// ---- the platform -------------------------------------------------------------

class Xbox : public x360::Platform {
public:
    Xbox() : xa_(0), master_(0), music_(0), musicVol_(0.7f), fadeVol_(1.0f), fadeStep_(0), pendingFade_(0),
             paused_(false), sfxVol_(0.7f) {}

    bool initAudio() {
        if (FAILED(XAudio2Create(&xa_, 0, XAUDIO2_DEFAULT_PROCESSOR))) { xa_ = 0; return false; }
        if (FAILED(xa_->CreateMasteringVoice(&master_))) { master_ = 0; return false; }
        static const char* const names[] = { "rotate", "place", "clear", "clear_big" };
        for (int i = 0; i < 4; ++i) {
            Sound& s = sfx_[names[i]];
            if (!loadWave(root() + "sfx\\" + names[i] + ".wav", s.wave)) continue;
            if (FAILED(xa_->CreateSourceVoice(&s.voice, &s.wave.fmt))) s.voice = 0;
        }
        return true;
    }

    // Once a frame: music fades, and a pending song switch.
    void tick() {
        if (!music_) { if (!pendingPath_.empty()) startPending(); return; }
        if (fadeStep_ != 0) {
            fadeVol_ += fadeStep_;
            if (fadeVol_ <= 0) {
                fadeVol_ = 0;
                fadeStep_ = 0;
                destroyMusic();
                if (!pendingPath_.empty()) startPending();
                return;
            }
            if (fadeVol_ >= 1) { fadeVol_ = 1; fadeStep_ = 0; }
        }
        music_->SetVolume(musicVol_ * fadeVol_);
    }

    // Platform
    bool readFile(const std::string& path, std::vector<uint8_t>& out) { return readWhole(path, out); }
    bool fileExists(const std::string& path) {
        return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
    }
    char separator() { return '\\'; }
    std::string root() { return "game:\\"; }
    const char* name() { return "Xbox 360"; }
    uint32_t ticks() { return GetTickCount(); }
    void log(const std::string& s) { OutputDebugStringA((s + "\n").c_str()); }
    bool hasAudio() { return master_ != 0; }

    void playMusic(const std::string& path, double fade) {
        if (!master_ || path == playingPath_) return;
        pendingPath_ = path;
        pendingFade_ = fade;
        if (music_) fadeStep_ = -float(1.0 / (60.0 * (fade > 0.05 ? fade : 0.05)));   // fade out first
        else startPending();
    }
    void stopMusic(double fade) {
        pendingPath_.clear();
        playingPath_.clear();
        if (music_) fadeStep_ = -float(1.0 / (60.0 * (fade > 0.05 ? fade : 0.05)));
    }
    void pauseMusic(bool on) {
        if (!music_ || on == paused_) return;
        paused_ = on;
        if (on) music_->Stop(0); else music_->Start(0);
    }
    void setMusicVolume(float v) { musicVol_ = v; if (music_) music_->SetVolume(musicVol_ * fadeVol_); }
    double musicPosition() {
        if (!music_) return 0;
        XAUDIO2_VOICE_STATE st;
        music_->GetState(&st);
        return double(st.SamplesPlayed) / double(musicWave_.fmt.nSamplesPerSec);
    }
    void playSfx(const std::string& name, float gain) {
        std::map<std::string, Sound>::iterator it = sfx_.find(name);
        if (it == sfx_.end() || !it->second.voice) return;
        Sound& s = it->second;
        s.voice->Stop(0);
        s.voice->FlushSourceBuffers();
        XAUDIO2_BUFFER b;
        ZeroMemory(&b, sizeof b);
        b.AudioBytes = UINT32(s.wave.data.size());
        b.pAudioData = &s.wave.data[0];
        b.Flags = XAUDIO2_END_OF_STREAM;
        s.voice->SubmitSourceBuffer(&b);
        s.voice->SetVolume(sfxVol_ * gain);
        s.voice->Start(0);
    }
    void setSfxVolume(float v) { sfxVol_ = v; }

private:
    struct Sound {
        Wave wave;
        IXAudio2SourceVoice* voice;
        Sound() : voice(0) {}
    };

    void destroyMusic() {
        if (!music_) return;
        music_->Stop(0);
        music_->DestroyVoice();
        music_ = 0;
        musicWave_ = Wave();
    }

    void startPending() {
        const std::string path = pendingPath_;
        pendingPath_.clear();
        destroyMusic();
        playingPath_ = path;
        if (!loadWave(path, musicWave_)) return;           // not rendered for this build: silence
        if (FAILED(xa_->CreateSourceVoice(&music_, &musicWave_.fmt))) { music_ = 0; return; }
        XAUDIO2_BUFFER b;
        ZeroMemory(&b, sizeof b);
        b.AudioBytes = UINT32(musicWave_.data.size());
        b.pAudioData = &musicWave_.data[0];
        b.LoopCount = XAUDIO2_LOOP_INFINITE;
        music_->SubmitSourceBuffer(&b);
        fadeVol_ = pendingFade_ > 0.05 ? 0.0f : 1.0f;
        fadeStep_ = pendingFade_ > 0.05 ? float(1.0 / (60.0 * pendingFade_)) : 0.0f;
        music_->SetVolume(musicVol_ * fadeVol_);
        paused_ = false;
        music_->Start(0);
    }

    IXAudio2* xa_;
    IXAudio2MasteringVoice* master_;
    IXAudio2SourceVoice* music_;
    Wave musicWave_;
    std::string playingPath_, pendingPath_;
    float musicVol_, fadeVol_, fadeStep_;
    double pendingFade_;
    bool paused_;
    std::map<std::string, Sound> sfx_;
    float sfxVol_;
};

// ---- video ------------------------------------------------------------------

const char* const kVertexShader =
    "struct V { float4 pos : POSITION; float2 uv : TEXCOORD0; };\n"
    "V main(V v) { return v; }\n";
const char* const kPixelShader =
    "sampler canvas : register(s0);\n"
    "float4 main(float2 uv : TEXCOORD0) : COLOR { return tex2D(canvas, uv); }\n";

struct Vertex { float x, y, z, w, u, v; };

class Screen {
public:
    Screen() : dev_(0), vs_(0), ps_(0), decl_(0), current_(0), cw_(0), ch_(0) { tex_[0] = tex_[1] = 0; }

    bool init() {
        IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
        if (!d3d) return false;
        D3DPRESENT_PARAMETERS pp;
        ZeroMemory(&pp, sizeof pp);
        pp.BackBufferWidth = kScreenW;
        pp.BackBufferHeight = kScreenH;
        pp.BackBufferFormat = D3DFMT_X8R8G8B8;
        pp.BackBufferCount = 1;
        pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
        if (FAILED(d3d->CreateDevice(0, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dev_)))
            return false;
        ID3DXBuffer* code = 0;
        ID3DXBuffer* errors = 0;
        if (FAILED(D3DXCompileShader(kVertexShader, UINT(strlen(kVertexShader)), NULL, NULL, "main", "vs_2_0", 0,
                                     &code, &errors, NULL)))
            return false;
        dev_->CreateVertexShader((const DWORD*)code->GetBufferPointer(), &vs_);
        code->Release();
        if (FAILED(D3DXCompileShader(kPixelShader, UINT(strlen(kPixelShader)), NULL, NULL, "main", "ps_2_0", 0,
                                     &code, &errors, NULL)))
            return false;
        dev_->CreatePixelShader((const DWORD*)code->GetBufferPointer(), &ps_);
        code->Release();
        static const D3DVERTEXELEMENT9 elements[] = {
            { 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
            { 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
            D3DDECL_END()
        };
        dev_->CreateVertexDeclaration(elements, &decl_);
        return vs_ && ps_ && decl_;
    }

    // The canvas, letterboxed to the screen with its aspect kept. Two
    // textures take turns, so the CPU never writes one the GPU is reading.
    void present(const x360::Canvas& c) {
        if (c.width() != cw_ || c.height() != ch_) {
            for (int i = 0; i < 2; ++i) {
                if (tex_[i]) tex_[i]->Release();
                tex_[i] = 0;
                dev_->CreateTexture(c.width(), c.height(), 1, 0, D3DFMT_LIN_A8R8G8B8, 0, &tex_[i], NULL);
            }
            cw_ = c.width();
            ch_ = c.height();
        }
        current_ ^= 1;
        IDirect3DTexture9* t = tex_[current_];
        D3DLOCKED_RECT lr;
        if (t && SUCCEEDED(t->LockRect(0, &lr, NULL, 0))) {
            const uint32_t* src = c.pixels();
            for (int y = 0; y < ch_; ++y)
                memcpy((uint8_t*)lr.pBits + y * lr.Pitch, src + y * cw_, size_t(cw_) * 4);
            t->UnlockRect(0);
        }
        const float sx = float(kScreenH) * cw_ / ch_ <= kScreenW ? float(kScreenH) * cw_ / ch_ / kScreenW : 1.0f;
        const float sy = float(kScreenH) * cw_ / ch_ <= kScreenW ? 1.0f : float(kScreenW) * ch_ / cw_ / kScreenH;
        const Vertex quad[4] = {
            { -sx, sy, 0, 1, 0, 0 }, { sx, sy, 0, 1, 1, 0 },
            { -sx, -sy, 0, 1, 0, 1 }, { sx, -sy, 0, 1, 1, 1 },
        };
        dev_->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
        dev_->SetVertexShader(vs_);
        dev_->SetPixelShader(ps_);
        dev_->SetVertexDeclaration(decl_);
        dev_->SetTexture(0, t);
        dev_->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        dev_->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        dev_->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        dev_->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        dev_->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(Vertex));
        dev_->SetTexture(0, NULL);
        dev_->Present(NULL, NULL, NULL, NULL);
    }

private:
    IDirect3DDevice9* dev_;
    IDirect3DVertexShader9* vs_;
    IDirect3DPixelShader9* ps_;
    IDirect3DVertexDeclaration9* decl_;
    IDirect3DTexture9* tex_[2];
    int current_, cw_, ch_;
};

// ---- the pad ------------------------------------------------------------------

// XInput buttons as the game's input names (SDL's controller names, as on
// the PC: scripts/ui/input.lua). Left / Right on the D-pad or the left
// stick repeat while held, like a keyboard (Down drops, so it doesn't).
struct Button { WORD mask; const char* name; bool repeats; };
const Button kButtons[] = {
    { XINPUT_GAMEPAD_DPAD_UP, "pad dpup", false }, { XINPUT_GAMEPAD_DPAD_DOWN, "pad dpdown", false },
    { XINPUT_GAMEPAD_DPAD_LEFT, "pad dpleft", true }, { XINPUT_GAMEPAD_DPAD_RIGHT, "pad dpright", true },
    { XINPUT_GAMEPAD_START, "pad start", false }, { XINPUT_GAMEPAD_BACK, "pad back", false },
    { XINPUT_GAMEPAD_LEFT_SHOULDER, "pad leftshoulder", false },
    { XINPUT_GAMEPAD_RIGHT_SHOULDER, "pad rightshoulder", false },
    { XINPUT_GAMEPAD_A, "pad a", false }, { XINPUT_GAMEPAD_B, "pad b", false },
    { XINPUT_GAMEPAD_X, "pad x", false }, { XINPUT_GAMEPAD_Y, "pad y", false },
};
const int kButtonCount = sizeof(kButtons) / sizeof(kButtons[0]);
const DWORD kRepeatDelay = 250, kRepeatRate = 50;   // ms

WORD padButtons() {
    XINPUT_STATE st;
    if (XInputGetState(0, &st) != ERROR_SUCCESS) return 0;
    WORD b = st.Gamepad.wButtons;
    const SHORT dead = 16000;   // the left stick as a D-pad
    if (st.Gamepad.sThumbLX < -dead) b |= XINPUT_GAMEPAD_DPAD_LEFT;
    if (st.Gamepad.sThumbLX > dead) b |= XINPUT_GAMEPAD_DPAD_RIGHT;
    if (st.Gamepad.sThumbLY > dead) b |= XINPUT_GAMEPAD_DPAD_UP;
    if (st.Gamepad.sThumbLY < -dead) b |= XINPUT_GAMEPAD_DPAD_DOWN;
    return b;
}

// Shows a Lua error instead of the game, so a broken scripts\ folder says why.
void showError(Screen& screen, x360::Canvas& c, const std::string& msg) {
    c.resize(640, 440);
    c.clear(0, 0, 128);
    c.text(16, 16, "Tetrispack stopped:", 1, 255, 255, 255, x360::Canvas::System);
    int y = 40;
    for (size_t p = 0; p < msg.size() && y < 420; p += 70, y += 16)
        c.text(16, y, msg.substr(p, 70), 1, 255, 255, 0, x360::Canvas::System);
    for (;;) screen.present(c);
}

}  // namespace

void __cdecl main() {
    Screen screen;
    if (!screen.init()) return;
    Xbox platform;
    platform.initAudio();
    x360::Game game(platform);
    if (!game.boot("data\\", "scripts\\")) showError(screen, game.canvas(), game.error());

    WORD held = 0;
    DWORD since[kButtonCount] = { 0 }, lastRepeat[kButtonCount] = { 0 };
    LARGE_INTEGER freq, last, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);
    while (!game.quitRequested()) {
        const WORD b = padButtons();
        const DWORD t = GetTickCount();
        for (int i = 0; i < kButtonCount; ++i) {
            const Button& k = kButtons[i];
            const bool down = (b & k.mask) != 0, was = (held & k.mask) != 0;
            bool fire = false;
            if (down && !was) { fire = true; since[i] = lastRepeat[i] = t; }
            else if (down && k.repeats && t - since[i] >= kRepeatDelay && t - lastRepeat[i] >= kRepeatRate) {
                fire = true;
                lastRepeat[i] = t;
            }
            if (fire && !game.key(k.name)) showError(screen, game.canvas(), game.error());
        }
        held = b;
        QueryPerformanceCounter(&now);
        double dt = double(now.QuadPart - last.QuadPart) / double(freq.QuadPart);
        last = now;
        if (dt > 0.1) dt = 0.1;
        if (!game.frame(dt)) showError(screen, game.canvas(), game.error());
        platform.tick();
        screen.present(game.canvas());
    }
}

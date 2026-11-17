#include "music/itwriter.h"

#include "music/snes_wave.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>

namespace music {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kC4 = 261.6255653;   // IT note C-5 plays MIDI key 60 (C4)

struct Writer {
    std::vector<uint8_t> b;
    void u8(int v) { b.push_back(uint8_t(v)); }
    void u16(int v) { u8(v & 0xFF); u8((v >> 8) & 0xFF); }
    void u32(uint32_t v) { for (int i = 0; i < 4; ++i) u8(int((v >> (8 * i)) & 0xFF)); }
    void str(const std::string& s, size_t n) {
        for (size_t i = 0; i < n; ++i) u8(i < s.size() ? s[i] : 0);
    }
    void put32(size_t at, uint32_t v) { for (int i = 0; i < 4; ++i) b[at + i] = uint8_t((v >> (8 * i)) & 0xFF); }
    void put16(size_t at, int v) { b[at] = uint8_t(v & 0xFF); b[at + 1] = uint8_t((v >> 8) & 0xFF); }
};

struct ItSample {
    std::string name;
    std::vector<int16_t> pcm;
    bool loop = false;
    uint32_t c5speed = 8363;
};

struct ItInstrument {
    std::string name;
    int pan = 32;                                // 0-64
    std::vector<std::pair<int, int>> envelope;   // (tick, level 0-64)
    int sustainNode = -1;
};

struct Cell {
    int note = -1, inst = 0, vol = -1, cmd = 0, val = 0;
    bool empty() const { return note < 0 && vol < 0 && cmd == 0; }
};

int16_t toPcm(float x) { return int16_t(std::lround(std::clamp(x, -1.0f, 1.0f) * 32000.0f)); }

// One looped cycle, re-synthesized at `len` samples from its harmonics below
// len / 2 (a band-limited resample, so short cycles don't alias).
std::vector<int16_t> resampleCycle(const std::vector<float>& cycle, int len) {
    const int n = int(cycle.size());
    std::vector<double> out(size_t(len), 0.0);
    for (int h = 1; h < len / 2 && h < n / 2; ++h) {
        double re = 0, im = 0;
        for (int i = 0; i < n; ++i) {
            re += cycle[size_t(i)] * std::cos(2 * kPi * h * i / n);
            im += cycle[size_t(i)] * std::sin(2 * kPi * h * i / n);
        }
        re *= 2.0 / n;
        im *= 2.0 / n;
        for (int i = 0; i < len; ++i)
            out[size_t(i)] += re * std::cos(2 * kPi * h * i / len) + im * std::sin(2 * kPi * h * i / len);
    }
    double peak = 1e-9;
    for (double v : out) peak = std::max(peak, std::fabs(v));
    std::vector<int16_t> pcm(static_cast<size_t>(len));
    for (int i = 0; i < len; ++i) pcm[size_t(i)] = toPcm(float(out[size_t(i)] / peak * 0.95));
    return pcm;
}

// A one-shot at a lower rate (averaging), cut to `seconds` with a short fade.
std::vector<int16_t> resampleOneShot(const std::vector<float>& data, double from, double to, double seconds) {
    const double step = from / to;
    const size_t n = std::min(size_t(seconds * to), size_t(double(data.size()) / step));
    std::vector<int16_t> pcm(n);
    const size_t fade = std::min(n, size_t(0.03 * to));
    for (size_t i = 0; i < n; ++i) {
        const double a = double(i) * step, b = a + step;
        double sum = 0, w = 0;
        for (size_t k = size_t(a); double(k) < b && k < data.size(); ++k) { sum += data[k]; w += 1; }
        float v = float(w > 0 ? sum / w : 0);
        if (i + fade >= n) v *= float(n - i) / float(fade + 1);
        pcm[i] = toPcm(v * 0.95f);
    }
    return pcm;
}

void writeEnvelope(Writer& w, const ItInstrument* inst) {
    const size_t start = w.b.size();
    if (!inst || inst->envelope.empty()) {
        w.u8(0); w.u8(0);
    } else {
        w.u8(1 | (inst->sustainNode >= 0 ? 4 : 0));
        w.u8(int(inst->envelope.size()));
    }
    w.u8(0); w.u8(0);                                         // loop
    const int sus = inst && inst->sustainNode >= 0 ? inst->sustainNode : 0;
    w.u8(sus); w.u8(sus);                                     // sustain loop: one node
    for (int i = 0; i < 25; ++i) {
        if (inst && size_t(i) < inst->envelope.size()) {
            w.u8(inst->envelope[size_t(i)].second);
            w.u16(inst->envelope[size_t(i)].first);
        } else {
            w.u8(0); w.u16(0);
        }
    }
    w.u8(0);
    (void)start;
}

std::vector<uint8_t> writeModule(const std::string& title, int rowsPerBeat, int speed, int tempo,
                                 const std::vector<ItInstrument>& instruments, const std::vector<ItSample>& samples,
                                 const std::vector<std::vector<std::vector<Cell>>>& patterns,  // [pattern][row][channel]
                                 int channels, const std::string& message) {
    Writer w;
    const int ord = int(patterns.size()) + 1;   // + the 255 end marker
    w.str("IMPM", 4);
    w.str(title, 26);
    w.u8(rowsPerBeat); w.u8(rowsPerBeat * 4);
    w.u16(ord); w.u16(int(instruments.size())); w.u16(int(samples.size())); w.u16(int(patterns.size()));
    w.u16(0x0214); w.u16(0x0214);
    w.u16(0x0D);                                 // stereo, instruments, linear slides
    w.u16(message.empty() ? 0 : 1);
    w.u8(128); w.u8(48); w.u8(speed); w.u8(tempo); w.u8(128); w.u8(0);
    w.u16(int(message.size() + (message.empty() ? 0 : 1)));
    const size_t msgOffsetAt = w.b.size();
    w.u32(0); w.u32(0);
    for (int c = 0; c < 64; ++c) w.u8(c < 8 ? 32 : 32 | 128);
    for (int c = 0; c < 64; ++c) w.u8(64);
    for (size_t p = 0; p < patterns.size(); ++p) w.u8(int(p));
    w.u8(255);
    const size_t insTable = w.b.size();
    for (size_t i = 0; i < instruments.size(); ++i) w.u32(0);
    const size_t smpTable = w.b.size();
    for (size_t i = 0; i < samples.size(); ++i) w.u32(0);
    const size_t patTable = w.b.size();
    for (size_t i = 0; i < patterns.size(); ++i) w.u32(0);

    if (!message.empty()) {
        w.put32(msgOffsetAt, uint32_t(w.b.size()));
        for (char c : message) w.u8(c == '\n' ? '\r' : c);
        w.u8(0);
    }

    for (size_t i = 0; i < instruments.size(); ++i) {
        const ItInstrument& in = instruments[i];
        w.put32(insTable + 4 * i, uint32_t(w.b.size()));
        w.str("IMPI", 4);
        w.str("", 12);
        w.u8(0);
        w.u8(0); w.u8(0); w.u8(0);               // NNA cut, no duplicate checks
        w.u16(0);                                // fade-out
        w.u8(0); w.u8(60);
        w.u8(128); w.u8(in.pan); w.u8(0); w.u8(0);
        w.u16(0x0214); w.u8(1); w.u8(0);
        w.str(in.name, 26);
        w.u8(0); w.u8(0); w.u8(0); w.u8(0); w.u16(0);
        for (int n = 0; n < 120; ++n) { w.u8(n); w.u8(int(i) + 1); }
        writeEnvelope(w, &in);
        writeEnvelope(w, nullptr);
        writeEnvelope(w, nullptr);
        w.u8(0); w.u8(0); w.u8(0); w.u8(0);
    }

    std::vector<size_t> dataAt(samples.size());
    for (size_t i = 0; i < samples.size(); ++i) {
        const ItSample& s = samples[i];
        w.put32(smpTable + 4 * i, uint32_t(w.b.size()));
        w.str("IMPS", 4);
        w.str("", 12);
        w.u8(0);
        w.u8(64);
        w.u8(1 | 2 | (s.loop ? 16 : 0));        // present, 16-bit, loop
        w.u8(64);
        w.str(s.name, 26);
        w.u8(1);                                 // signed
        w.u8(32);                                // no default pan (bit 7 off)
        w.u32(uint32_t(s.pcm.size()));
        w.u32(0); w.u32(s.loop ? uint32_t(s.pcm.size()) : 0);
        w.u32(s.c5speed);
        w.u32(0); w.u32(0);
        dataAt[i] = w.b.size();
        w.u32(0);
        w.u8(0); w.u8(0); w.u8(0); w.u8(0);
    }

    for (size_t p = 0; p < patterns.size(); ++p) {
        w.put32(patTable + 4 * p, uint32_t(w.b.size()));
        const size_t head = w.b.size();
        w.u16(0); w.u16(int(patterns[p].size())); w.u32(0);
        const size_t start = w.b.size();
        for (const auto& row : patterns[p]) {
            for (int c = 0; c < channels; ++c) {
                const Cell& cell = row[size_t(c)];
                if (cell.empty()) continue;
                int mask = 0;
                if (cell.note >= 0) mask |= 1;
                if (cell.inst > 0) mask |= 2;
                if (cell.vol >= 0) mask |= 4;
                if (cell.cmd) mask |= 8;
                w.u8((c + 1) | 0x80);
                w.u8(mask);
                if (mask & 1) w.u8(cell.note);
                if (mask & 2) w.u8(cell.inst);
                if (mask & 4) w.u8(cell.vol);
                if (mask & 8) { w.u8(cell.cmd); w.u8(cell.val); }
            }
            w.u8(0);
        }
        w.put16(head, int(w.b.size() - start));
    }

    for (size_t i = 0; i < samples.size(); ++i) {
        w.put32(dataAt[i], uint32_t(w.b.size()));
        for (int16_t v : samples[i].pcm) w.u16(uint16_t(v));
    }
    return w.b;
}

}  // namespace

// The coarsest grid (rows per beat) on which every note starts and ends
// within a tenth of a row: fewer rows, smaller patterns in the SNES's RAM.
static int pickRows(const Song& song, double bpm) {
    for (int r : {4, 6, 8, 12}) {
        bool ok = true;
        for (const Note& n : song.notes) {
            for (double t : {n.start, n.start + n.length}) {
                const double x = t * bpm / 60.0 * r;
                if (std::fabs(x - std::round(x)) > 0.1) { ok = false; break; }
            }
            if (!ok) break;
        }
        if (ok) return r;
    }
    return 12;
}

std::vector<uint8_t> songToIt(const Song& song, const ItSettings& settings, ItReport* report) {
    ItSettings st = settings;
    if (st.rowsPerBeat <= 0) st.rowsPerBeat = pickRows(song, st.bpm);
    const double rowsPerSec = st.bpm / 60.0 * st.rowsPerBeat;
    const double ticksPerSec = st.bpm * 0.4;                 // IT tempo = BPM
    const int speed = 24 / st.rowsPerBeat;                   // ticks per row

    // The highest key each instrument plays: loops get a cycle short enough
    // that the S-DSP stays under its 128 kHz playback limit.
    std::vector<int> maxKey(song.instruments.size(), 0);
    for (const Note& n : song.notes) {
        const Instrument& in = song.instruments[size_t(n.instrument)];
        maxKey[size_t(n.instrument)] = std::max(maxKey[size_t(n.instrument)], in.fixedKey >= 0 ? in.fixedKey : n.key);
    }

    std::vector<ItInstrument> instruments;
    std::vector<ItSample> samples;
    ItReport rep;
    for (size_t i = 0; i < song.instruments.size(); ++i) {
        const Instrument& in = song.instruments[i];
        const SnesWave wave = snesWave(in.wave);
        ItSample s;
        s.name = "wave" + std::to_string(int(in.wave));
        if (wave.loop) {
            const double hz = kC4 * std::pow(2.0, (maxKey[i] - 60) / 12.0);
            int len = 64;
            while (len > 16 && hz * len > 120000.0) len /= 2;
            s.pcm = resampleCycle(wave.data, len);
            s.loop = true;
            s.c5speed = uint32_t(std::lround(kC4 * len));
        } else {
            s.pcm = resampleOneShot(wave.data, wave.rate, st.oneShotRate, st.oneShotSeconds);
            s.c5speed = uint32_t(std::lround(st.oneShotRate * std::pow(2.0, (60 - wave.baseKey) / 12.0)));
        }
        rep.sampleBytes += s.pcm.size() * 2;
        samples.push_back(std::move(s));

        ItInstrument it;
        it.name = "inst" + std::to_string(i + 1);
        it.pan = std::clamp(int(std::lround(32 + in.pan * 32)), 0, 64);
        // The software envelope as IT nodes (1-256 ticks apart).
        auto ticks = [&](float seconds) { return std::clamp(int(std::lround(seconds * ticksPerSec)), 1, 256); };
        int t = 0;
        if (in.env.attack * ticksPerSec < 1.0) {
            it.envelope.push_back({0, 64});
        } else {
            it.envelope.push_back({0, 0});
            t += ticks(in.env.attack);
            it.envelope.push_back({t, 64});
        }
        const int sus = std::clamp(int(std::lround(in.env.sustain * 64)), 0, 64);
        if (sus < 64) {
            t += ticks(in.env.decay);
            it.envelope.push_back({t, sus});
        }
        it.sustainNode = int(it.envelope.size()) - 1;
        t += ticks(in.env.release);
        it.envelope.push_back({t, 0});
        instruments.push_back(std::move(it));
    }

    // Notes into channels.
    std::vector<Note> notes = song.notes;
    std::stable_sort(notes.begin(), notes.end(), [](const Note& a, const Note& b) { return a.start < b.start; });
    const int totalRows = std::max(1, int(std::lround(song.length * rowsPerSec)));
    std::vector<std::vector<Cell>> grid(size_t(totalRows), std::vector<Cell>(size_t(st.channels)));
    std::vector<int> end(size_t(st.channels), 0), lastInst(size_t(st.channels), -1), echoOn(size_t(st.channels), -1);
    std::vector<int> pendingOff(size_t(st.channels), -1), priority(size_t(st.channels), 0);
    for (const Note& n : notes) {
        const Instrument& in = song.instruments[size_t(n.instrument)];
        const int row = int(std::lround(n.start * rowsPerSec));
        if (row >= totalRows) continue;
        const int stop = std::max(row + 1, int(std::lround((n.start + n.length) * rowsPerSec)));
        int ch = -1;
        for (int c = 0; c < st.channels; ++c)       // free, same instrument
            if (end[size_t(c)] <= row && lastInst[size_t(c)] == n.instrument) { ch = c; break; }
        if (ch < 0)
            for (int c = 0; c < st.channels; ++c)   // free, the longest idle
                if (end[size_t(c)] <= row && (ch < 0 || end[size_t(c)] < end[size_t(ch)])) ch = c;
        if (ch < 0) {                                // busy: the lowest priority, ending soonest
            for (int c = 0; c < st.channels; ++c)
                if (grid[size_t(row)][size_t(c)].note < 0 &&
                    (ch < 0 || priority[size_t(c)] < priority[size_t(ch)] ||
                     (priority[size_t(c)] == priority[size_t(ch)] && end[size_t(c)] < end[size_t(ch)])))
                    ch = c;
            if (ch < 0 || priority[size_t(ch)] > in.priority) { ++rep.dropped; continue; }
        }
        const size_t c = size_t(ch);
        if (pendingOff[c] >= 0 && pendingOff[c] < row && pendingOff[c] < totalRows && grid[size_t(pendingOff[c])][c].note < 0)
            grid[size_t(pendingOff[c])][c].note = 255;
        Cell& cell = grid[size_t(row)][c];
        cell.note = std::clamp(in.fixedKey >= 0 ? in.fixedKey : n.key, 0, 119);
        cell.inst = n.instrument + 1;
        cell.vol = std::clamp(int(std::lround(64.0 * std::clamp(n.velocity * in.gain, 0.0f, 1.0f))), 0, 64);
        const int echo = in.echo > 0.15f ? 1 : 0;
        if (echo != echoOn[c]) {
            cell.cmd = 19;                           // S0x: echo for this channel
            cell.val = echo ? 0x02 : 0x01;
            echoOn[c] = echo;
        }
        end[c] = stop;
        lastInst[c] = n.instrument;
        pendingOff[c] = stop;
        priority[c] = in.priority;
        ++rep.notes;
    }
    for (int c = 0; c < st.channels; ++c)
        if (pendingOff[size_t(c)] >= 0 && pendingOff[size_t(c)] < totalRows &&
            grid[size_t(pendingOff[size_t(c)])][size_t(c)].note < 0)
            grid[size_t(pendingOff[size_t(c)])][size_t(c)].note = 255;

    // Patterns of up to 16 beats (IT allows 32-200 rows).
    const int perPattern = (200 / (4 * st.rowsPerBeat)) * 4 * st.rowsPerBeat;
    std::vector<std::vector<std::vector<Cell>>> patterns;
    for (int r = 0; r < totalRows; r += perPattern) {
        const int rows = std::max(32, std::min(perPattern, totalRows - r));
        std::vector<std::vector<Cell>> p(size_t(rows), std::vector<Cell>(size_t(st.channels)));
        for (int k = 0; k < rows && r + k < totalRows; ++k) p[size_t(k)] = grid[size_t(r + k)];
        patterns.push_back(std::move(p));
    }

    // SNESMOD's echo settings go in the song message.
    std::string msg;
    bool anyEcho = false;
    for (const Instrument& in : song.instruments) anyEcho = anyEcho || in.echo > 0.15f;
    if (anyEcho) {
        const int edl = std::clamp(int(std::lround(song.echoDelay * 1000.0 / 16.0)), 1, st.maxEchoDelay);
        msg = "[[SNESMOD]]\nEDL " + std::to_string(edl) + "\nEFB " +
              std::to_string(int(std::lround(song.echoFeedback * 127))) + "\nEVOL " +
              std::to_string(int(std::lround(song.echoVolume * 127)));
    }

    rep.rows = totalRows;
    rep.rowsPerBeat = st.rowsPerBeat;
    rep.patterns = int(patterns.size());
    if (report) *report = rep;
    return writeModule(st.title, st.rowsPerBeat, speed, std::clamp(int(std::lround(st.bpm)), 32, 255),
                       instruments, samples, patterns, st.channels, msg);
}

std::vector<uint8_t> soundsToIt(const std::vector<std::vector<int16_t>>& sounds, const std::vector<std::string>& names,
                                int rate, const std::string& title) {
    std::vector<ItInstrument> instruments;
    std::vector<ItSample> samples;
    for (size_t i = 0; i < sounds.size(); ++i) {
        ItSample s;
        s.name = i < names.size() ? names[i] : "sfx";
        s.pcm = sounds[i];
        s.c5speed = uint32_t(rate);
        ItInstrument in;
        in.name = s.name;
        samples.push_back(std::move(s));
        instruments.push_back(std::move(in));
    }
    // One empty pattern: the module only carries the samples.
    std::vector<std::vector<std::vector<Cell>>> patterns(1, std::vector<std::vector<Cell>>(64, std::vector<Cell>(1)));
    return writeModule(title, 4, 6, 125, instruments, samples, patterns, 1, "");
}

}  // namespace music

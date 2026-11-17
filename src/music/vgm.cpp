#include "music/vgm.h"


namespace music {

void VgmWriter::waitUntil(uint64_t at) {
    if (at <= now_) return;
    uint64_t gap = at - now_;
    now_ = at;
    while (gap > 0) {
        if (gap == 735) { data_.push_back(0x62); gap = 0; break; }
        if (gap == 882) { data_.push_back(0x63); gap = 0; break; }
        if (gap <= 16) { data_.push_back(uint8_t(0x70 + gap - 1)); gap = 0; break; }
        const uint16_t n = uint16_t(gap > 65535 ? 65535 : gap);
        data_.push_back(0x61);
        data_.push_back(uint8_t(n & 0xFF));
        data_.push_back(uint8_t(n >> 8));
        gap -= n;
    }
}

void VgmWriter::ym2612(int port, uint8_t reg, uint8_t val, uint64_t at) {
    waitUntil(at);
    data_.push_back(port ? 0x53 : 0x52);
    data_.push_back(reg);
    data_.push_back(val);
    ++commands_;
}

void VgmWriter::psg(uint8_t val, uint64_t at) {
    waitUntil(at);
    data_.push_back(0x50);
    data_.push_back(val);
    ++commands_;
}

void VgmWriter::loopHere(uint64_t at) {
    waitUntil(at);
    loopOffset_ = data_.size();
    loopAt_ = now_;
    looped_ = true;
}

std::vector<uint8_t> VgmWriter::finish(uint64_t at) {
    waitUntil(at);
    std::vector<uint8_t> out(0x40, 0);
    auto put32 = [&](size_t off, uint32_t v) {
        for (int i = 0; i < 4; ++i) out[off + i] = uint8_t(v >> (8 * i));
    };
    out[0] = 'V'; out[1] = 'g'; out[2] = 'm'; out[3] = ' ';
    const uint32_t total = uint32_t(0x40 + data_.size() + 1);
    put32(0x04, total - 4);                       // EOF offset
    put32(0x08, 0x150);                           // version 1.50
    put32(0x0C, kPsgClock);
    put32(0x18, uint32_t(now_));                  // total samples
    if (looped_) {
        put32(0x1C, uint32_t(0x40 + loopOffset_ - 0x1C));
        put32(0x20, uint32_t(now_ - loopAt_));
    }
    put32(0x24, 60);                              // NTSC
    out[0x28] = 0x09; out[0x29] = 0x00;           // SN76489 feedback (Sega)
    out[0x2A] = 16;                               // shift register width
    put32(0x2C, kYm2612Clock);
    put32(0x34, 0x40 - 0x34);                     // data offset
    out.insert(out.end(), data_.begin(), data_.end());
    out.push_back(0x66);                          // end of data
    return out;
}

}  // namespace music

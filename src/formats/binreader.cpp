#include "formats/binreader.h"

#include <fstream>

namespace tp {

void BinReader::need(size_t off, size_t n) const {
    if (off > data_.size() || n > data_.size() - off)
        throw FormatError("read past end of data (offset " + std::to_string(off) +
                          ", " + std::to_string(n) + " bytes, size " +
                          std::to_string(data_.size()) + ")");
}

void BinReader::seek(size_t pos) {
    need(pos, 0);
    pos_ = pos;
}

uint8_t BinReader::u8At(size_t off) const {
    need(off, 1);
    return data_[off];
}

uint16_t BinReader::u16At(size_t off) const {
    need(off, 2);
    return static_cast<uint16_t>(data_[off] | (data_[off + 1] << 8));
}

uint32_t BinReader::u32At(size_t off) const {
    need(off, 4);
    return static_cast<uint32_t>(data_[off]) | (static_cast<uint32_t>(data_[off + 1]) << 8) |
           (static_cast<uint32_t>(data_[off + 2]) << 16) | (static_cast<uint32_t>(data_[off + 3]) << 24);
}

uint8_t BinReader::u8() { auto v = u8At(pos_); pos_ += 1; return v; }
uint16_t BinReader::u16() { auto v = u16At(pos_); pos_ += 2; return v; }
uint32_t BinReader::u32() { auto v = u32At(pos_); pos_ += 4; return v; }

std::string BinReader::pstring() {
    const uint8_t n = u8();
    auto b = bytes(pos_, n);
    pos_ += n;
    return std::string(b.begin(), b.end());
}

std::span<const uint8_t> BinReader::bytes(size_t off, size_t n) const {
    need(off, n);
    return data_.subspan(off, n);
}

std::vector<uint8_t> readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw FormatError("cannot open " + path);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

}  // namespace tp

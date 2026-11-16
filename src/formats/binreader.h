// Bounds-checked little-endian reads over a byte buffer. Every read past the
// end throws FormatError, so a truncated or corrupt file can't crash the port.
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace tp {

struct FormatError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class BinReader {
public:
    explicit BinReader(std::span<const uint8_t> data) : data_(data) {}

    size_t size() const { return data_.size(); }
    size_t tell() const { return pos_; }
    void seek(size_t pos);
    void skip(size_t n) { seek(pos_ + n); }

    uint8_t  u8();
    uint16_t u16();
    uint32_t u32();
    int32_t  i32() { return static_cast<int32_t>(u32()); }

    uint8_t  u8At(size_t off) const;
    uint16_t u16At(size_t off) const;
    uint32_t u32At(size_t off) const;

    // A byte-length-prefixed string (Pascal style, as NE name tables use).
    std::string pstring();
    std::span<const uint8_t> bytes(size_t off, size_t n) const;

private:
    void need(size_t off, size_t n) const;
    std::span<const uint8_t> data_;
    size_t pos_ = 0;
};

std::vector<uint8_t> readFile(const std::string& path);

}  // namespace tp

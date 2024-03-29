#pragma once
#include <string>
#include <vector>

#include "VgmHeader.h"

// Holds binary data and lets us read stuff from it easily
// - just holds the whole file in memory
// - decompresses from GZip transparently
// - assumes little-endian byte ordering
class BinaryData
{
public:
    BinaryData() = default;
    BinaryData(const std::string& filename);

    void seek(unsigned int offset);

    uint8_t peek() const;
    uint8_t read_uint8();
    uint16_t read_uint16();
    uint32_t read_uint24();
    uint32_t read_uint32();

    std::string read_ascii_string(uint32_t length);
    std::wstring read_null_terminated_utf16_string();

    std::vector<uint8_t> read_range(uint32_t length);

    [[nodiscard]]
    unsigned int size() const
    {
        return static_cast<unsigned int>(_data.size());
    }

    [[nodiscard]]
    unsigned int offset() const
    {
        return _offset;
    }

    [[nodiscard]]
    bool at_end() const
    {
        return _offset == size();
    }

    void copy_range(std::vector<unsigned char>& destination, uint32_t startIndex, uint32_t byteCount) const;

    // Write string with no terminator
    void write_unterminated_ascii_string(const std::string& s);

    // Write numbers of given sizes
    void write_uint8(uint8_t i);
    void write_uint32(uint32_t i);
    void write_uint24(uint32_t i);
    void write_uint16(uint16_t i);

    // Write as UTF-16 with terminating 0
    void write_terminated_utf16_string(const std::wstring& s);

    // Write an arbitrary blob of data
    void write_range(const std::vector<unsigned char>& data);

    // Save to disk
    void save(const std::string& filename) const;

    // Get the whole buffer as a const ref
    [[nodiscard]] const std::vector<uint8_t>& buffer() const
    {
        return _data;
    }

    // Reset to empty
    void reset();

private:
    void check_write_space(size_t size);

    std::vector<uint8_t> _data;
    unsigned int _offset{};
};

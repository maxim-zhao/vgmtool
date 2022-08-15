#pragma once
#include <string>
#include <vector>

#include "VgmFile.h"
#include "VgmHeader.h"

// Holds binary data and lets us read stuff from it easily
// - just holds the whole file in memory
// - decompresses from GZip transparently
// - assumes little-endian byte ordering
// - writing to come later
class BinaryData
{
public:
    BinaryData(const std::string& filename);

    void seek(unsigned int offset);

    uint8_t read_byte();
    uint16_t read_word();
    uint32_t read_long();

    std::string read_utf8_string(int length);
    std::wstring read_null_terminated_utf16_string();

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

    void copy_range(std::vector<unsigned char>& destination, uint32_t startIndex, uint32_t byteCount) const;

private:
    std::vector<uint8_t> _data;
    unsigned int _offset{};
};

#pragma once
#include <string>
#include <vector>

// Holds binary data and lets us read stuff from it easily
class BinaryData
{
public:
    BinaryData() = default;

    void read_from_file(const std::string& filename);

    void seek(const unsigned offset);

    uint8_t read_byte();
    uint16_t read_word();
    uint32_t read_long();

    std::string read_utf8_string(int length);
    std::wstring read_null_terminated_utf16_string();

private:
    std::vector<uint8_t> _data;
    int _offset{};
};

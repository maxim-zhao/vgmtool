#include "BinaryData.h"

#include <stdexcept>

#include "utils.h"

void BinaryData::read_from_file(const std::string& filename)
{
    _data.clear();
    Utils::load_file(_data, filename);
}

void BinaryData::seek(const unsigned offset)
{
    if (offset < 0 || static_cast<size_t>(offset) >= _data.size())
    {
        throw std::runtime_error(Utils::format("Cannot seek to offset 0x%x as it is beyond the data size (%llu bytes)", offset, _data.size()));
    }
    _offset = offset;
}

uint8_t BinaryData::read_byte()
{
    return _data[_offset++];
}

uint16_t BinaryData::read_word()
{
    uint16_t result = read_byte();
    result |= read_byte() << 8;
    return result;
}

uint32_t BinaryData::read_long()
{
    uint16_t result = read_byte();
    result |= read_byte() << 8;
    result |= read_byte() << 16;
    result |= read_byte() << 24;
    return result;
}

std::string BinaryData::read_utf8_string(const int length)
{
    std::string result;
    result.reserve(length);
    for (auto i = 0; i < length; ++i)
    {
        result.append(1, static_cast<char>(read_byte()));
    }
    return result;
}

std::wstring BinaryData::read_null_terminated_utf16_string()
{
    std::wstring result;

    while (true)
    {
        const auto w = read_word();
        if (w == 0)
        {
            return result;
        }
        result.append(1, static_cast<wchar_t>(w));
    }
}


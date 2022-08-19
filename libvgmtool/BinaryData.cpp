#include "BinaryData.h"

#include <stdexcept>
#include <fstream>

#include "utils.h"

BinaryData::BinaryData(const std::string& filename)
{
    Utils::load_file(_data, filename);
}

void BinaryData::seek(const unsigned int offset)
{
    if (offset < 0 || static_cast<size_t>(offset) > _data.size())
    {
        throw std::runtime_error(Utils::format("Cannot seek to offset 0x%x as it is beyond the data size (%llu bytes)", offset, _data.size()));
    }
    _offset = offset;
}

uint8_t BinaryData::read_uint8()
{
    return _data[_offset++];
}

uint16_t BinaryData::read_uint16()
{
    uint16_t result = read_uint8();
    result |= read_uint8() << 8;
    return result;
}

uint32_t BinaryData::read_uint32()
{
    uint32_t result = read_uint8();
    result |= read_uint8() << 8;
    result |= read_uint8() << 16;
    result |= read_uint8() << 24;
    return result;
}

std::string BinaryData::read_ascii_string(const int length)
{
    std::string result;
    result.reserve(length);
    for (auto i = 0; i < length; ++i)
    {
        result.append(1, static_cast<char>(read_uint8()));
    }
    return result;
}

std::wstring BinaryData::read_null_terminated_utf16_string()
{
    std::wstring result;

    while (true)
    {
        const auto w = read_uint16();
        if (w == 0)
        {
            return result;
        }
        result.append(1, static_cast<wchar_t>(w));
    }
}

void BinaryData::copy_range(std::vector<unsigned char>& destination, uint32_t startIndex, uint32_t byteCount) const
{
    std::copy_n(_data.begin() + startIndex, byteCount, std::back_inserter(destination));
}

void BinaryData::check_write_space(const size_t size)
{
    const auto requiredSize = _offset + size;
    if (requiredSize < _data.size())
    {
        // Nothing to do
        return;
    }
    // Else make space
    _data.resize(requiredSize);
}

void BinaryData::write_unterminated_ascii_string(const std::string& s)
{
    check_write_space(s.size());

    for (const uint8_t c : s)
    {
        _data[_offset++] = c;
    }
}

void BinaryData::write_uint32(const uint32_t i)
{
    check_write_space(4);
    _data[_offset++] = (i >> 0) & 0xff;
    _data[_offset++] = (i >> 8) & 0xff;
    _data[_offset++] = (i >> 16) & 0xff;
    _data[_offset++] = (i >> 24) & 0xff;
}

void BinaryData::write_uint16(const uint16_t i)
{
    check_write_space(2);
    _data[_offset++] = (i >> 0) & 0xff;
    _data[_offset++] = (i >> 8) & 0xff;
}

void BinaryData::write_uint8(uint8_t i)
{
    check_write_space(1);
    _data[_offset++] = i;
}

void BinaryData::write_terminated_utf16_string(const std::wstring& s)
{
    check_write_space(s.size() * 2 + 2);
    for (const auto value : s)
    {
        write_uint16(value);
    }
    write_uint16(0);
}

void BinaryData::add_range(const std::vector<unsigned char>& data)
{
    check_write_space(data.size());
    std::ranges::copy(data, _data.begin() + _offset);
    _offset += static_cast<uint32_t>(data.size());
}

void BinaryData::save(const std::string& filename) const
{
    std::ofstream f(filename, std::ios::binary | std::ios::trunc | std::ios::out);
    f.write(reinterpret_cast<const char*>(_data.data()), static_cast<std::streamsize>(_data.size()));
    f.close();
}


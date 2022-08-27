#include "BcdVersion.h"

#include <format>
#include <stdexcept>

#include "utils.h"
#include "BinaryData.h"

void BcdVersion::from_binary(BinaryData& data)
{
    const auto i = data.read_uint32();
    if ((i & 0xffff0000u) != 0)
    {
        throw std::runtime_error(std::format("Invalid version: non-zero padding: \"{:04x}\"", i));
    }
    // We parse the version as two BCD bytes.
    _major = from_bcd((i >> 8u) & 0xffu);
    _minor = from_bcd((i >> 0u) & 0xffu);
}

int BcdVersion::from_bcd(uint32_t bcd)
{
    int result = 0;
    int multiplier = 1;
    for (auto i = 0; i < 2; ++i)
    {
        const auto digit = bcd & 0xf;
        if (digit > 9)
        {
            throw std::runtime_error(std::format("Invalid BCD number \"{:02x}\"", bcd));
        }
        result += static_cast<int>(digit) * multiplier;
        multiplier *= 10;
        bcd >>= 4;
    }
    return result;
}

void BcdVersion::to_binary(BinaryData& data) const
{
    // We convert the version to BCD
    const uint32_t bcd =
        ((_minor % 10) << 0) |
        ((_minor / 10) << 8) |
        ((_major % 10) << 16) |
        ((_major / 10) << 24);
    data.write_uint32(bcd);
}

bool BcdVersion::at_least(const int major, const int minor) const
{
    return _major > major ||
        (_major == major && _minor >= minor);
}

std::string BcdVersion::string()
{
    return std::format("{}.{:02}", _major, _minor);
}

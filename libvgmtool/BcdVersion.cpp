#include "BcdVersion.h"

#include <stdexcept>

#include "utils.h"

int BcdVersion::from_bcd(int bcd)
{
    int result = 0;
    int multiplier = 1;
    for (auto i = 0; i < 2; ++i)
    {
        const auto digit = bcd & 0xf;
        if (digit > 9)
        {
            throw std::runtime_error(Utils::format("Invalid BCD number \"%02x\"", bcd));
        }
        result += digit * multiplier;
        multiplier *= 10;
        bcd >>= 4;
    }
    return result;
}

void BcdVersion::from_binary(BinaryData& data)
{
    const auto i = data.read_long();
    if ((i & 0xffff0000u) != 0)
    {
        throw std::runtime_error(Utils::format("Invalid version: non-zero padding: \"%04x\"", i));
    }
    // We parse the version as two BCD bytes.
    _major = from_bcd((i >> 8) & 0xff);
    _minor = from_bcd((i >> 0) & 0xff);
}

#include "VgmHeader.h"

#include <stdexcept>

#include "utils.h"
#include "BinaryData.h"

namespace
{
    const std::string VGM_IDENT("Vgm ");
    constexpr auto EOF_DELTA = 0x04u;
    constexpr auto GD3_DELTA = 0x14u;
    constexpr auto LOOP_DELTA = 0x1cu;
    constexpr auto DATA_DELTA = 0x34u;
    constexpr auto DATA_DEFAULT = 0x40u;
}

void VgmHeader::from_binary(BinaryData& data)
{
    // https://www.smspower.org/uploads/Music/vgmspec160.txt

    // Check ident
    if (const auto& ident = data.read_utf8_string(4); ident != VGM_IDENT)
    {
        throw std::runtime_error(Utils::format("Invalid GD3 header ident \"%s\"", ident.c_str()));
    }

    _eofOffset = data.read_long() + EOF_DELTA;
    if (data.size() != _eofOffset)
    {
        throw std::runtime_error(Utils::format("Invalid EOF offset %x", _eofOffset));
    }

    _version.from_binary(data);
    if (_version.major() != 1)
    {
        throw std::runtime_error(Utils::format("Unsupported version %d.%02d", _version.major(), _version.minor()));
    }

    const auto offsetC = data.read_long();
    _clocks[Chip::SN76489] = offsetC & 0x7fffffffu;
    _flags[Flag::IsT6W28] = (offsetC & 0x80000000u) != 0u;
    _clocks[Chip::YM2413] = data.read_long();

    _gd3Offset = data.read_long();
    if (_gd3Offset != 0)
    {
        _gd3Offset += GD3_DELTA;
    }
    _sampleCount = data.read_long();
    _loopOffset = data.read_long();
    if (_loopOffset != 0)
    {
        _loopOffset += LOOP_DELTA;
    }
    _loopSampleCount = data.read_long();

    // VGM 1.01
    if (_version.minor() >= 1)
    {
        _frameRate = data.read_long();
    }
    else
    {
        // Default for VGM < 1.01
        _frameRate = 0;
    }

    // VGM 1.10
    if (_version.minor() >= 10)
    {
        _sn76489Feedback = data.read_word();
        _sn76489ShiftRegisterWidth = data.read_byte();

        if (_version.minor() >= 51)
        {
            // VGM 1.51 added these, in a previously always 0 byte from VGM 1.10
            _sn76489Flags = data.read_byte();
        }
        else
        {
            _sn76489Flags = 0;
            // Skip byte
            data.seek(data.offset() + 1);
        }

        _clocks[Chip::YM2612] = data.read_long();
        _clocks[Chip::YM2151] = data.read_long();
    }
    else
    {
        _sn76489Feedback = 0x0009;
        _sn76489ShiftRegisterWidth = 16;
        _sn76489Flags = 0;
        // Shared clock for older VGMs.
        // TODO autodetect these from the data
        _clocks[Chip::YM2612] = _clocks[Chip::YM2413];
        _clocks[Chip::YM2151] = _clocks[Chip::YM2413];
    }

    // VGM 1.50
    if (_version.minor() >= 50)
    {
        _dataOffset = data.read_long() + DATA_DELTA;
    }
    else
    {
        // Older VGMs always have it at 0x40
        _dataOffset = DATA_DEFAULT;
    }
    if (_version.minor() >= 51)
    {
        // VGM 1.51
        _clocks[Chip::SegaPCM] = data.read_long();
        _segaPcmInterfaceRegister = data.read_long();
        _clocks[Chip::RF5C68] = data.read_long();
        _clocks[Chip::YM2203] = data.read_long();
        _clocks[Chip::YM2608] = data.read_long();
        const auto ym2610Data = data.read_long();
        _clocks[Chip::YM2610] = ym2610Data & 0x7fffffffu;
        _flags[Flag::IsYM2610B] = (ym2610Data & 0x80000000u) != 0u;
        _clocks[Chip::YM3812] = data.read_long();
        _clocks[Chip::YM3526] = data.read_long();
        _clocks[Chip::Y8950] = data.read_long();
        _clocks[Chip::YMF262] = data.read_long();
        _clocks[Chip::YMF271] = data.read_long();
        _clocks[Chip::YMZ280B] = data.read_long();
        _clocks[Chip::RF5C164] = data.read_long();
        _clocks[Chip::PWM] = data.read_long();
        _clocks[Chip::AY8910] = data.read_long();
        _ay8910ChipType = data.read_byte();
        _ay8910Flags = data.read_byte();
        _ym2203ay8910Flags = data.read_byte();
        _ym2608ay8910Flags = data.read_byte();

        if (_version.minor() >= 60)
        {
            _volumeModifier = data.read_byte();
            // Skip a byte
            data.seek(data.offset() + 1);
            _loopBase = data.read_byte();
        }
        else
        {
            _volumeModifier = 0;
            _loopBase = 0;
            // Skip three bytes
            data.seek(data.offset() + 3);
        }

        _loopModifier = data.read_byte();
    }
    else
    {
        _segaPcmInterfaceRegister = 0u;
        _ay8910ChipType = 0;
        _ay8910Flags = 0;
        _ym2203ay8910Flags = 0;
        _ym2608ay8910Flags = 0;
        _loopModifier = 0;
    }

    // Remaining header bytes must be 0
    while (data.offset() != _dataOffset)
    {
        if (const auto b = data.read_byte(); b != 0)
        {
            throw std::runtime_error(Utils::format("Invalid header byte at offset 0x%x: value %02x", data.offset() - 1,
                b));
        }
    }
}

uint32_t VgmHeader::gd3_offset() const
{
    return _gd3Offset;
}

uint32_t VgmHeader::data_offset() const
{
    return _dataOffset;
}

uint32_t VgmHeader::eof_offset() const
{
    return _eofOffset;
}

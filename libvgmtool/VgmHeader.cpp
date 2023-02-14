#include "VgmHeader.h"

#include <format>
#include <sstream>
#include <stdexcept>
#include <ranges>

#include "BinaryData.h"
#include "KeyValuePrinter.h"

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
    if (const auto& ident = data.read_ascii_string(4); ident != VGM_IDENT)
    {
        throw std::runtime_error(std::format("Invalid GD3 header ident \"{}\"", ident));
    }

    _eofOffset = data.read_uint32() + EOF_DELTA;
    if (_eofOffset == EOF_DELTA)
    {
        // Assume it's the whole file
        _eofOffset = data.size();
    }
    // Do we have more (or less) data than expected?
    // This seems to often be wrong due to bugs in tools, so we ignore it
    /*
    if (data.size() != _eofOffset)
    {
        throw std::runtime_error(Utils::format(
            "EOF offset is 0x%x but file size is 0x%x",
            _eofOffset,
            data.size()));
    }
    */

    _version.from_binary(data);
    if (_version.major() != 1)
    {
        throw std::runtime_error(std::format("Unsupported version {}", _version.string()));
    }

    const auto offsetC = data.read_uint32();
    _clocks[Chip::SN76489] = offsetC & 0x7fffffffu;
    _flags[Flag::IsT6W28] = (offsetC & 0x80000000u) != 0u;
    _clocks[Chip::YM2413] = data.read_uint32();

    _gd3Offset = data.read_uint32();
    if (_gd3Offset != 0)
    {
        _gd3Offset += GD3_DELTA;
    }
    _sampleCount = data.read_uint32();
    _loopOffset = data.read_uint32();
    if (_loopOffset != 0)
    {
        _loopOffset += LOOP_DELTA;
    }
    _loopSampleCount = data.read_uint32();

    // VGM 1.01
    if (_version.at_least(1, 1))
    {
        _frameRate = data.read_uint32();
    }
    else
    {
        // Default for VGM < 1.01
        _frameRate = 0;
    }

    // VGM 1.10
    if (_version.at_least(1, 10))
    {
        _sn76489Feedback = data.read_uint16();
        _sn76489ShiftRegisterWidth = data.read_uint8();

        if (_version.at_least(1, 51))
        {
            // VGM 1.51 added these, in a previously always 0 byte from VGM 1.10
            _sn76489Flags = data.read_uint8();
        }
        else
        {
            _sn76489Flags = 0;
            // Skip byte
            data.seek(data.offset() + 1);
        }

        _clocks[Chip::YM2612] = data.read_uint32();
        _clocks[Chip::YM2151] = data.read_uint32();
    }
    else
    {
        _sn76489Feedback = 0x0009;
        _sn76489ShiftRegisterWidth = 16;
        _sn76489Flags = 0;
        // Shared clock for older VGMs.
        // TODO autodetect these from the data?
        _clocks[Chip::YM2612] = _clocks[Chip::YM2413];
        _clocks[Chip::YM2151] = _clocks[Chip::YM2413];
    }

    // VGM 1.50
    if (_version.at_least(1, 50))
    {
        _dataOffset = data.read_uint32() + DATA_DELTA;
    }
    else
    {
        // Older VGMs always have it at 0x40
        _dataOffset = DATA_DEFAULT;
    }
    if (_version.at_least(1, 51))
    {
        // VGM 1.51
        _clocks[Chip::SegaPCM] = data.read_uint32();
        _segaPcmInterfaceRegister = data.read_uint32();
        _clocks[Chip::RF5C68] = data.read_uint32();
        _clocks[Chip::YM2203] = data.read_uint32();
        _clocks[Chip::YM2608] = data.read_uint32();
        const auto ym2610Data = data.read_uint32();
        _clocks[Chip::YM2610] = ym2610Data & 0x7fffffffu;
        _flags[Flag::IsYM2610B] = (ym2610Data & 0x80000000u) != 0u;
        _clocks[Chip::YM3812] = data.read_uint32();
        _clocks[Chip::YM3526] = data.read_uint32();
        _clocks[Chip::Y8950] = data.read_uint32();
        _clocks[Chip::YMF262] = data.read_uint32();
        _clocks[Chip::YMF278B] = data.read_uint32();
        _clocks[Chip::YMF271] = data.read_uint32();
        _clocks[Chip::YMZ280B] = data.read_uint32();
        _clocks[Chip::RF5C164] = data.read_uint32();
        _clocks[Chip::PWM] = data.read_uint32();
        _clocks[Chip::AY8910] = data.read_uint32();
        _ay8910ChipType = static_cast<ay8910_chip_types>(data.read_uint8());
        _ay8910Flags = data.read_uint8();
        _ym2203ay8910Flags = data.read_uint8();
        _ym2608ay8910Flags = data.read_uint8();

        // Pull out "dual chip" bits
        check_second_chip_bit(Chip::YM2413);
        check_second_chip_bit(Chip::YM2612);
        check_second_chip_bit(Chip::YM2151);
        check_second_chip_bit(Chip::YM2203);
        check_second_chip_bit(Chip::YM2608);
        check_second_chip_bit(Chip::YM2610);
        check_second_chip_bit(Chip::YM3812);
        check_second_chip_bit(Chip::YM3526);
        check_second_chip_bit(Chip::Y8950);
        check_second_chip_bit(Chip::YMF262);
        check_second_chip_bit(Chip::YMF278B);
        check_second_chip_bit(Chip::YMF271);
        check_second_chip_bit(Chip::YMZ280B);

        if (_version.at_least(1, 60))
        {
            _volumeModifier = data.read_uint8();
            // Skip a byte
            data.seek(data.offset() + 1);
            _loopBase = data.read_uint8();
        }
        else
        {
            _volumeModifier = 0;
            _loopBase = 0;
            // Skip three bytes
            data.seek(data.offset() + 3);
        }

        _loopModifier = data.read_uint8();
    }
    else
    {
        _segaPcmInterfaceRegister = 0u;
        _ay8910ChipType = static_cast<ay8910_chip_types>(0);
        _ay8910Flags = 0;
        _ym2203ay8910Flags = 0;
        _ym2608ay8910Flags = 0;
        _loopModifier = 0;
    }

    // Remaining header bytes must be 0
    while (data.offset() != _dataOffset)
    {
        if (const auto b = data.read_uint8(); b != 0)
        {
            throw std::runtime_error(std::format("Invalid header byte at offset 0x{:x}: value {:02x}", data.offset() - 1, b));
        }
    }
}

void VgmHeader::to_binary(BinaryData& data) const
{
    data.seek(0);
    // First the ident
    data.write_unterminated_ascii_string(VGM_IDENT);
    data.write_uint32(_eofOffset - EOF_DELTA);
    _version.to_binary(data);
    data.write_uint32(clock(Chip::SN76489));
    data.write_uint32(clock(Chip::YM2413));
    data.write_uint32(_gd3Offset == 0u ? 0u : _gd3Offset - GD3_DELTA);
    data.write_uint32(_sampleCount);
    data.write_uint32(_loopOffset - LOOP_DELTA);
    data.write_uint32(_loopSampleCount);
    if (_version.at_least(1, 1))
    {
        data.write_uint32(_frameRate);
    }
    if (_version.at_least(1, 10))
    {
        data.write_uint16(_sn76489Feedback);
        data.write_uint8(_sn76489ShiftRegisterWidth);
        if (_version.at_least(1, 51))
        {
            data.write_uint8(_sn76489Flags);
        }
        else
        {
            data.write_uint8(0);
        }
        data.write_uint32(clock(Chip::YM2612));
        data.write_uint32(clock(Chip::YM2151));
    }

    if (_version.at_least(1, 50))
    {
        data.write_uint32(_dataOffset - DATA_DELTA);
        // TODO this ought to be figured out based on the version? Maybe we should safely round-trip any unknown bytes?
    }

    if (_version.at_least(1, 51))
    {
        data.write_uint32(clock(Chip::SegaPCM));
        data.write_uint32(_segaPcmInterfaceRegister);
        data.write_uint32(clock(Chip::RF5C68));
        data.write_uint32(clock(Chip::YM2203));
        data.write_uint32(clock(Chip::YM2608));
        data.write_uint32(clock(Chip::YM2610) | (flag(Flag::IsYM2610B) ? 1u << 31 : 0u));
        data.write_uint32(clock(Chip::YM3812));
        data.write_uint32(clock(Chip::YM3526));
        data.write_uint32(clock(Chip::Y8950));
        data.write_uint32(clock(Chip::YMF262));
        data.write_uint32(clock(Chip::YMF278B));
        data.write_uint32(clock(Chip::YMF271));
        data.write_uint32(clock(Chip::YMZ280B));
        data.write_uint32(clock(Chip::RF5C164));
        data.write_uint32(clock(Chip::PWM));
        data.write_uint32(clock(Chip::AY8910));
        data.write_uint8(static_cast<uint8_t>(_ay8910ChipType));
        data.write_uint8(_ay8910Flags);
        data.write_uint8(_ym2203ay8910Flags);
        data.write_uint8(_ym2608ay8910Flags);

        if (_version.at_least(1, 60))
        {
            data.write_uint8(_volumeModifier);
            // Reserved byte
            data.write_uint8(0);
            data.write_uint8(_loopBase);
        }
        else
        {
            // Three reserved bytes
            data.write_uint8(0);
            data.write_uint8(0);
            data.write_uint8(0);
        }

        data.write_uint8(_loopModifier);
        // Reserved byte
        data.write_uint8(0);
    }

    // Pad as needed
    const uint32_t minimumSize = _version.at_least(1, 50) ? 0x80 : 0x40;
    while (data.offset() < minimumSize)
    {
        data.write_uint8(0);
    }
}

uint32_t VgmHeader::clock(const Chip chip) const
{
    const auto it = _clocks.find(chip);
    if (it == _clocks.end())
    {
        return 0;
    }
    return it->second;
}

void VgmHeader::set_clock(const Chip chip, const uint32_t value)
{
    _clocks[chip] = value;
}

bool VgmHeader::flag(const Flag flag) const
{
    const auto it = _flags.find(flag);
    if (it == _flags.end())
    {
        return false;
    }
    return it->second;
}

void VgmHeader::check_second_chip_bit(Chip chip)
{
    // Set flag from bit 30
    _haveSecondChip[chip] = (_clocks[chip] & (1 << 30)) != 0;
    // Clear bits 30-31
    _clocks[chip] &= 0b00111111'11111111'11111111'11111111;
}

std::string VgmHeader::write_to_text() const
{
    KeyValuePrinter printer;

    // VGM 1.00
    printer.add("End-of-file offset", std::format("{:#010x} (absolute)", eof_offset()));
    printer.add("VGM version", version().string());
    printer.add("SN76489 clock", std::format("{} Hz", clock(Chip::SN76489)));
    printer.add(
        _version.at_least(1, 10) ? "YM2413 clock" : "FM clock",
        std::format("{} Hz", clock(Chip::YM2413)));
    printer.add("GD3 tag offset", std::format("{:#010x} (absolute)", gd3_offset()));
    printer.add("Total length", std::format("{} samples ({:.2f}s)", sample_count(), sample_count() / 44100.0));
    printer.add("Loop point offset", std::format("{:#08x} (absolute)", loop_offset()));
    printer.add("Loop length", std::format("{} samples ({:.2f}s)", loop_sample_count(), loop_sample_count() / 44100.0));
    if (_version.at_least(1, 1))
    {
        printer.add("Recording rate", std::format("{} Hz", frame_rate()));
    }
    if (_version.at_least(1, 10))
    {
        printer.add("SN76489 noise feedback", std::format("{:#06x}", sn76489_feedback()));
        printer.add("SN76489 shift register width", std::format("{} bits", sn76489_shift_register_width()));
    }
    if (_version.at_least(1, 51))
    {
        printer.add("SN76489 flags", std::format("{:#04x}", sn76489_flags()));
    }
    if (_version.at_least(1, 10))
    {
        printer.add("YM2612 clock", std::format("{} Hz", clock(Chip::YM2612)));
        printer.add("YM2151 clock", std::format("{} Hz", clock(Chip::YM2612)));
    }
    if (_version.at_least(1, 50))
    {
        printer.add("VGM data offset", std::format("{:#010x}", data_offset()));
    }
    if (_version.at_least(1, 51))
    {
        printer.add("Sega PCM clock", std::format("{} Hz", clock(Chip::SegaPCM)));
        printer.add("Sega PCM interface register", std::format("{:#010x}", sega_pcm_interface_register()));
        printer.add("RF5C68 clock", std::format("{} Hz", clock(Chip::RF5C68)));
        printer.add("YM2203 clock", std::format("{} Hz", clock(Chip::YM2203)));
        printer.add("YM2608 clock", std::format("{} Hz", clock(Chip::YM2608)));
        printer.add("YM2610/YM2610B clock", std::format("{} Hz", clock(Chip::YM2610)));
        printer.add("YM2610/YM2610B identity", flag(Flag::IsYM2610B) ? "YM2610" : "YM2610B");
        printer.add("YM3812 clock", std::format("{} Hz", clock(Chip::YM3812)));
        printer.add("YM3526 clock", std::format("{} Hz", clock(Chip::YM3526)));
        printer.add("Y8950 clock", std::format("{} Hz", clock(Chip::Y8950)));
        printer.add("YMF262 clock", std::format("{} Hz", clock(Chip::YMF262)));
        printer.add("YMF278B clock", std::format("{} Hz", clock(Chip::YMF278B)));
        printer.add("YMF271 clock", std::format("{} Hz", clock(Chip::YMF271)));
        printer.add("YMZ280B clock", std::format("{} Hz", clock(Chip::YMZ280B)));
        printer.add("RF5C164 clock", std::format("{} Hz", clock(Chip::RF5C164)));
        printer.add("YM2608 clock", std::format("{} Hz", clock(Chip::YM2608)));
        printer.add("PWM clock", std::format("{} Hz", clock(Chip::PWM)));
        printer.add("AY8910 clock", std::format("{} Hz", clock(Chip::AY8910)));
        printer.add("AY8910 chip type", std::format("{}", static_cast<int>(ay8910_chip_type())));
        printer.add("AY8910 flags", std::format("{}", ay8910_flags()));
        printer.add("YM2203/AY8910 flags", std::format("{}", ym2203_ay8910_flags()));
        printer.add("YM2608/AY8910 flags", std::format("{}", ym2608_ay8910_flags()));
    }
    if (_version.at_least(1, 60))
    {
        printer.add("Volume modifier", std::format("{:#04x}", volume_modifier()));
        printer.add("Loop base", std::format("{}", loop_base()));
    }
    if (_version.at_least(1, 51))
    {
        printer.add("Loop modifier", std::format("{}", loop_modifier()));
    }

    return printer.string();
}

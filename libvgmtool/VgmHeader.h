#pragma once
#include "BcdVersion.h"
#include <unordered_map>
#include <cstdint>
class BinaryData;

class VgmHeader
{
public:
    VgmHeader() = default;
    ~VgmHeader() = default;

    VgmHeader(const VgmHeader& other) = delete;
    VgmHeader(VgmHeader&& other) noexcept = delete;
    VgmHeader& operator=(const VgmHeader& other) = delete;
    VgmHeader& operator=(VgmHeader&& other) noexcept = delete;

    void from_binary(BinaryData& data);

    [[nodiscard]] std::string write_to_text() const;

    enum class ay8910_chip_types
    {
        AY8910 = 0x00,
        AY8912 = 0x01,
        AY8913 = 0x02,
        AY8930 = 0x03,
        YM2149 = 0x10,
        YM3439 = 0x11,
        YMZ284 = 0x12,
        YMZ294 = 0x13
    };

    [[nodiscard]] uint32_t eof_offset() const
    {
        return _eofOffset;
    }

    [[nodiscard]] BcdVersion version() const
    {
        return _version;
    }

    [[nodiscard]] uint32_t gd3_offset() const
    {
        return _gd3Offset;
    }

    [[nodiscard]] uint32_t sample_count() const
    {
        return _sampleCount;
    }

    [[nodiscard]] uint32_t loop_offset() const
    {
        return _loopOffset;
    }

    [[nodiscard]] uint32_t loop_sample_count() const
    {
        return _loopSampleCount;
    }

    [[nodiscard]] uint32_t frame_rate() const
    {
        return _frameRate;
    }

    [[nodiscard]] uint16_t sn76489_feedback() const
    {
        return _sn76489Feedback;
    }

    [[nodiscard]] uint8_t sn76489_shift_register_width() const
    {
        return _sn76489ShiftRegisterWidth;
    }

    [[nodiscard]] uint8_t sn76489_flags() const
    {
        return _sn76489Flags;
    }

    [[nodiscard]] uint32_t data_offset() const
    {
        return _dataOffset;
    }

    [[nodiscard]] uint32_t sega_pcm_interface_register() const
    {
        return _segaPcmInterfaceRegister;
    }

    [[nodiscard]] ay8910_chip_types ay8910_chip_type() const
    {
        return _ay8910ChipType;
    }

    [[nodiscard]] uint8_t ay8910_flags() const
    {
        return _ay8910Flags;
    }

    [[nodiscard]] uint8_t ym2203_ay8910_flags() const
    {
        return _ym2203ay8910Flags;
    }

    [[nodiscard]] uint8_t ym2608_ay8910_flags() const
    {
        return _ym2608ay8910Flags;
    }

    [[nodiscard]] uint8_t volume_modifier() const
    {
        return _volumeModifier;
    }

    [[nodiscard]] uint8_t loop_base() const
    {
        return _loopBase;
    }

    [[nodiscard]] uint8_t loop_modifier() const
    {
        return _loopModifier;
    }

    void set_eof_offset(const uint32_t eofOffset)
    {
        _eofOffset = eofOffset;
    }

    void set_version(const BcdVersion version)
    {
        _version = version;
    }

    void set_gd3_offset(const uint32_t gd3Offset)
    {
        _gd3Offset = gd3Offset;
    }

    void set_sample_count(const uint32_t sampleCount)
    {
        _sampleCount = sampleCount;
    }

    void set_loop_offset(const uint32_t loopOffset)
    {
        _loopOffset = loopOffset;
    }

    void set_loop_sample_count(const uint32_t loopSampleCount)
    {
        _loopSampleCount = loopSampleCount;
    }

    void set_frame_rate(const uint32_t frameRate)
    {
        _frameRate = frameRate;
    }

    void set_sn76489_feedback(const uint16_t sn76489Feedback)
    {
        _sn76489Feedback = sn76489Feedback;
    }

    void set_sn76489_shift_register_width(const uint8_t sn76489ShiftRegisterWidth)
    {
        _sn76489ShiftRegisterWidth = sn76489ShiftRegisterWidth;
    }

    void set_sn76489_flags(const uint8_t sn76489Flags)
    {
        _sn76489Flags = sn76489Flags;
    }

    void set_data_offset(const uint32_t dataOffset)
    {
        _dataOffset = dataOffset;
    }

    void set_sega_pcm_interface_register(const uint32_t segaPcmInterfaceRegister)
    {
        _segaPcmInterfaceRegister = segaPcmInterfaceRegister;
    }

    void set_ay8910_chip_type(const ay8910_chip_types ay8910ChipType)
    {
        _ay8910ChipType = ay8910ChipType;
    }

    void set_ay8910_flags(const uint8_t ay8910Flags)
    {
        _ay8910Flags = ay8910Flags;
    }

    void set_ym2203_ay8910_flags(const uint8_t ym2203Ay8910Flags)
    {
        _ym2203ay8910Flags = ym2203Ay8910Flags;
    }

    void set_ym2608_ay8910_flags(const uint8_t ym2608Ay8910Flags)
    {
        _ym2608ay8910Flags = ym2608Ay8910Flags;
    }

    void set_volume_modifier(const uint8_t volumeModifier)
    {
        _volumeModifier = volumeModifier;
    }

    void set_loop_base(const uint8_t loopBase)
    {
        _loopBase = loopBase;
    }

    void set_loop_modifier(const uint8_t loopModifier)
    {
        _loopModifier = loopModifier;
    }

    void to_binary(BinaryData& data) const;

    enum class Chip
    {
        Nothing,
        SN76489,
        YM2413,
        YM2612,
        YM2151,
        SegaPCM,
        RF5C68,
        YM2203,
        YM2608,
        YM2610,
        YM3812,
        YM3526,
        Y8950,
        YMF262,
        YMF278B,
        YMF271,
        YMZ280B,
        RF5C164,
        PWM,
        AY8910,
        GenericDAC
    };

    enum class Flag
    {
        IsT6W28,
        IsYM2610B
    };

    [[nodiscard]] uint32_t clock(Chip chip) const;
    void set_clock(Chip chip, uint32_t value);

    [[nodiscard]] bool flag(Flag flag) const;

private:
    void check_second_chip_bit(::VgmHeader::Chip chip);

    uint32_t _eofOffset{};
    BcdVersion _version{};
    std::unordered_map<Chip, uint32_t> _clocks;
    std::unordered_map<Flag, bool> _flags;
    uint32_t _gd3Offset{};
    uint32_t _sampleCount{};
    uint32_t _loopOffset{};
    uint32_t _loopSampleCount{};
    uint32_t _frameRate{};
    uint16_t _sn76489Feedback{};
    uint8_t _sn76489ShiftRegisterWidth{};
    uint8_t _sn76489Flags{};
    uint32_t _dataOffset{};
    uint32_t _segaPcmInterfaceRegister{};

    ay8910_chip_types _ay8910ChipType{};
    uint8_t _ay8910Flags{};
    uint8_t _ym2203ay8910Flags{};
    uint8_t _ym2608ay8910Flags{};
    uint8_t _volumeModifier{};
    uint8_t _loopBase{};
    uint8_t _loopModifier{};
    std::unordered_map<Chip, bool> _haveSecondChip{};
};

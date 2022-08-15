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

    void from_binary(BinaryData& data);
    [[nodiscard]] uint32_t gd3_offset() const;
    [[nodiscard]] uint32_t data_offset() const;
    [[nodiscard]] uint32_t eof_offset() const;

private:
    enum class Chip
    {
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
        YMF271,
        YMZ280B,
        RF5C164,
        PWM,
        AY8910
    };

    enum class Flag
    {
        IsT6W28,
        IsYM2610B
    };

    uint32_t _eofOffset;
    BcdVersion _version;
    std::unordered_map<Chip, uint32_t> _clocks;
    std::unordered_map<Flag, bool> _flags;
    uint32_t _gd3Offset;
    uint32_t _sampleCount;
    uint32_t _loopOffset;
    uint32_t _loopSampleCount;
    uint32_t _frameRate;
    uint16_t _sn76489Feedback;
    uint8_t _sn76489ShiftRegisterWidth;
    uint8_t _sn76489Flags;
    uint32_t _dataOffset;
    uint32_t _segaPcmInterfaceRegister;
    int _ay8910ChipType; // TODO: make this an enum
    uint8_t _ay8910Flags;
    uint8_t _ym2203ay8910Flags;
    uint8_t _ym2608ay8910Flags;
    uint8_t _volumeModifier;
    uint8_t _loopBase;
    uint8_t _loopModifier;
};

#pragma once
#include <cstdint>

enum class Chip: std::uint8_t
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

#include "SN76489State.h"

#include <format>
#include <iostream>

#include "utils.h"
#include "VgmCommands.h"

SN76489State::SN76489State(const VgmHeader& header)
    : _clockRate(header.clock(VgmHeader::Chip::SN76489))
{
    _noiseSpeedDescriptions =
    {
        make_noise_description("high", 0),
        make_noise_description("med", 1),
        make_noise_description("low", 2),
        "ch 2"
    };
    for (int i = 0; i < 15; ++i)
    {
        const int dB = i * 2;
        _volumeDescriptions.emplace_back(std::format("{:#x} = {:2} dB = {:3.0f}%", i, dB, Utils::db_to_percent(dB)));
    }
    _volumeDescriptions.emplace_back(std::format("{:#x} =  ∞ dB = {:3.0f}%", 15, 0.0));
}

void SN76489State::add_with_text(const VgmCommands::ICommand* pCommand, std::ostream& s)
{
    if (const auto* pStereo = dynamic_cast<const VgmCommands::GGStereo*>(pCommand); pStereo != nullptr)
    {
        add(pStereo);
        s << "Stereo: " << print_stereo_mask(_stereoMask);
        return;
    }
    if (const auto* p = dynamic_cast<const VgmCommands::SN76489*>(pCommand); p != nullptr)
    {
        add(p);
        if ((p->value() & 0b10000000) == 0)
        {
            s << "Data:       ";
        }
        else
        {
            s << "Latch/data: ";
        }
        const int registerValue = _registers[_latchedRegisterIndex];
        switch (_latchedRegisterIndex)
        {
        case 0:
        case 2:
        case 4: // Tone registers
            {
                const int channel = _latchedRegisterIndex / 2;
                double frequencyHz = tone_length_to_hz(registerValue);
                s << "Tone ch " << channel
                    << std::format(" -> {:#05x}", registerValue)
                    << std::format(" = {:8.2f} Hz", frequencyHz)
                    << " = " << Utils::note_name(frequencyHz);
                return;
            }
        case 6: // Noise
            {
                const char* noiseType = (registerValue & 0b100) == 0
                    ? "synchronous"
                    : "white";
                const int noiseSpeed = registerValue & 0b011;
                s << "Noise: " << noiseType << ", " << _noiseSpeedDescriptions[noiseSpeed];
                return;
            }
        default: // Volume
            {
                const int channel = _latchedRegisterIndex / 2;
                s << "Attenuation ch " << channel << " -> " << _volumeDescriptions[registerValue];
                return;
            }
        } // end switch
    }
    throw std::runtime_error("Unexpected command type");
}

void SN76489State::add(const VgmCommands::GGStereo* pStereo)
{
    _stereoMask = pStereo->value();
}

void SN76489State::add(const VgmCommands::SN76489* pCommand)
{
    if (const auto value = pCommand->value();
        (value & 0b10000000) != 0)
    {
        // ReSharper disable CommentTypo
        // Latch/data byte %1nnvdddd
        // nnv = register index
        // dddd = low 4 bits of data
        // ReSharper restore CommentTypo
        _latchedRegisterIndex = (value & 0b01110000) >> 4;
        _registers[_latchedRegisterIndex] &= 0b1111110000;
        _registers[_latchedRegisterIndex] |= value & 0b1111;
    }
    else
    {
        // ReSharper disable once CommentTypo
        // Data byte %0ddddddd
        if (_latchedRegisterIndex % 2 == 0 && _latchedRegisterIndex < 5)
        {
            // Tone register, apply to high bits
            _registers[_latchedRegisterIndex] &= 0b0000001111;
            _registers[_latchedRegisterIndex] |= (value & 0b111111) << 4;
        }
        else
        {
            // Other register, truncate to 4 bits and replace
            _registers[_latchedRegisterIndex] = value & 0b1111;
        }
    }
}


std::string SN76489State::print_stereo_mask(const uint8_t mask)
{
    std::string bits("012N012N");
    for (int i = 0; i < 8; ++i)
    {
        if ((mask >> i & 1) == 0)
        {
            bits[i] = '-';
        }
    }
    return bits;
}

double SN76489State::tone_length_to_hz(const int length) const
{
    if (length == 0)
    {
        return 0.0;
    }
    return static_cast<double>(_clockRate) / 32.0 / length;
}

std::string SN76489State::make_noise_description(const char* prefix, int shift) const
{
    return std::format(
        "{} ({}Hz)",
        prefix,
        _clockRate / 32 / (16 << shift));
}

#include "SN76489State.h"

#include <format>
#include <format>

#include "utils.h"

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
        const double amplitude = std::pow(10, -0.1 * i);
        const int percentage = static_cast<int>(std::round(amplitude * 100));
        _volumeDescriptions.emplace_back(std::format("{:#x} = -{}dB = {}%", i, dB, percentage));
    }
    _volumeDescriptions.emplace_back(std::format("{:#x} = {}%", 15, 0));
}

std::string SN76489State::add_with_text(const VgmCommands::ICommand* pCommand)
{
    if (const auto* pStereo = dynamic_cast<const VgmCommands::GGStereo*>(pCommand); pStereo != nullptr)
    {
        add(pStereo);
        return std::format("SN76489: Stereo: {}", print_stereo_mask(_stereoMask));
    }
    if (const auto* p = dynamic_cast<const VgmCommands::SN76489*>(pCommand); p != nullptr)
    {
        add(p);
        const char* description = (p->value() & 0b10000000) == 0
            ? "SN76489: Data:       "
            : "SN76489: Latch/data: ";
        const int registerValue = _registers[_latchedRegisterIndex];
        switch (_latchedRegisterIndex)
        {
        case 0:
        case 2:
        case 4: // Tone registers
            {
                const int channel = _latchedRegisterIndex / 2;
                double frequencyHz = tone_length_to_hz(registerValue);
                return std::format("{} Tone ch {} -> 0x{:03x} = {:8.2f} Hz = {}",
                    description,
                    channel, // Channel
                    registerValue, // Value
                    frequencyHz, // Frequency
                    Utils::note_name(frequencyHz) // Note
                );
            }
        case 6: // Noise
            {
                const char* noiseType = (registerValue & 0b100) == 0
                    ? "synchronous"
                    : "white";
                const int noiseSpeed = registerValue & 0b011;
                return std::format(
                    "Noise: {}, {}",
                    noiseType,
                    _noiseSpeedDescriptions[noiseSpeed]);
            }
        default: // Volume
            {
                const int channel = _latchedRegisterIndex / 2;
                return std::format(
                    "SN76489: Volume: ch {} -> {}",
                    channel,
                    registerValue,
                    _volumeDescriptions[registerValue]);
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
    const auto value = pCommand->value();
    if ((value & 0b10000000) != 0)
    {
        // Latch/data byte %1nnvdddd
        // nnv = register index
        // dddd = low 4 bits of data
        _latchedRegisterIndex = (value & 0b01110000) >> 4;
        _registers[_latchedRegisterIndex] &= 0b1111110000;
        _registers[_latchedRegisterIndex] |= value & 0b1111;
    }
    else
    {
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


std::string SN76489State::print_stereo_mask(uint8_t mask)
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

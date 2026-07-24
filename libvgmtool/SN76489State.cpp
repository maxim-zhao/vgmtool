#include "SN76489State.h"

#include <format>
#include <iostream>

#include "Chip.h"
#include "CommandStream.h"
#include "utils.h"
#include "VgmCommands.h"

SN76489State::SN76489State(const VgmHeader& header)
    : _clockRate(header.clock(Chip::SN76489))
{
}

void SN76489State::to_text(std::ostream& s, const std::shared_ptr<const VgmCommands::ICommand>& pCommand)
{
    prepare_text();
    if (const auto ggStereo = std::dynamic_pointer_cast<const VgmCommands::GGStereo>(pCommand))
    {
        add(ggStereo);
        std::string bits("012N012N");
        for (int i = 0; i < 8; ++i)
        {
            if ((_stereoMask >> i & 1) == 0)
            {
                bits[i] = '-';
            }
        }
        s << "Stereo: " << bits;
        return;
    }
    if (const auto sn76489 = std::dynamic_pointer_cast<const VgmCommands::SN76489>(pCommand))
    {
        add(sn76489);
        if ((sn76489->value() & 0b10000000) == 0)
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
                const auto channel = _latchedRegisterIndex / 2;
                double frequencyHz = registerValue == 0
                    ? 0.0
                    : static_cast<double>(_clockRate) / 32.0 / registerValue;
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
                const auto channel = _latchedRegisterIndex / 2;
                s << "Attenuation ch " << channel << " -> " << _volumeDescriptions[registerValue];
                return;
            }
        } // end switch
    }
    throw std::runtime_error("Unexpected command type");
}

void SN76489State::copy_to_command_stream(
    CommandStream& stream,
    std::shared_ptr<IChipState> lastWrittenPsgStatePtr,
    const bool fullImage) const
{
    auto lastWrittenPsgState = std::dynamic_pointer_cast<SN76489State>(lastWrittenPsgStatePtr);
    if (fullImage || _stereoMask != lastWrittenPsgState->_stereoMask)
    {
        auto ggStereo = std::make_shared<VgmCommands::GGStereo>();
        ggStereo->set_value(_stereoMask);
        stream.commands().emplace_back(ggStereo);
        lastWrittenPsgState->_stereoMask = _stereoMask;
    }

    for (std::size_t i = 0; i < _registers.size(); ++i)
    {
        if (fullImage || _registers[i] != lastWrittenPsgState->_registers[i])
        {
            const auto channel = i / 2;
            const auto isTone = ((i % 2) == 0) && (i != 6); // Channels 0, 2, 4 are tone channels
            const auto isVolume = (i % 2) == 1; // Channels 1, 3, 5, 7 are volume channels
            const auto mask = (channel << 5) | (isVolume
                ? 0b10000
                : 0);

            // All registers have a first byte, whether they're tone, noise or volume
            auto command1 = std::make_shared<VgmCommands::SN76489>();
            command1->set_value(static_cast<uint8_t>(0b10000000 | mask | (_registers[i] & 0b1111)));
            stream.commands().push_back(command1);
            if (isTone)
            {
                // Then there's a second data byte
                auto command2 = std::make_shared<VgmCommands::SN76489>();
                // ReSharper disable once CommentTypo
                // Data byte %0ddddddd
                command2->set_value(static_cast<uint8_t>(_registers[i] >> 4));
                stream.commands().push_back(command2);
            }
            lastWrittenPsgState->_registers[i] = _registers[i];
        }
    }
}

std::shared_ptr<IChipState> SN76489State::clone() const
{
    return std::make_shared<SN76489State>(*this);
}

void SN76489State::prepare_text()
{
    if (!_noiseSpeedDescriptions.empty())
    {
        return;
    }

    auto makeNoiseDescription = [&](const char* prefix, const int shift)
    {
        return std::format(
            "{} ({}Hz)",
            prefix,
            _clockRate / 32 / (16 << shift));
    };

    _noiseSpeedDescriptions =
    {
        makeNoiseDescription("high", 0),
        makeNoiseDescription("med", 1),
        makeNoiseDescription("low", 2),
        "ch 2"
    };
    for (int i = 0; i < 15; ++i)
    {
        const int dB = i * 2;
        _volumeDescriptions.emplace_back(std::format("{:#x} = {:2} dB = {:3.0f}%", i, dB, Utils::db_to_percent(dB)));
    }
    _volumeDescriptions.emplace_back(std::format("{:#x} =  ∞ dB = {:3.0f}%", 15, 0.0));
}


void SN76489State::add(const std::shared_ptr<const VgmCommands::ICommand>& command)
{
    if (const auto pStereo = std::dynamic_pointer_cast<const VgmCommands::GGStereo>(command))
    {
        _stereoMask = pStereo->value();
    }
    else if (const auto pCommand = std::dynamic_pointer_cast<const VgmCommands::SN76489>(command))
    {
        if (const auto value = pCommand->value();
            (value & 0b10000000) != 0)
        {
            // ReSharper disable once CommentTypo
            // Latch/data byte %1nnvdddd
            // nnv = register index
            // dddd = low 4 bits of data
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
    else
    {
        throw std::exception("Unhandled command type");
    }
}

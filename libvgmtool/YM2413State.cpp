#include "YM2413State.h"

#include <array>
#include <format>
#include <sstream>
#include <unordered_set>

#include "CommandStream.h"
#include "utils.h"
#include "VgmHeader.h"
#include "VgmCommands.h"

namespace
{
    const std::unordered_set<uint8_t> VALID_REGISTERS
    {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, // Custom instrument
        0x0e, // Rhythm control
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, // F-number low 8 bits
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, // F-number high bit, block, key, sustain
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, // Instrument, volume
    };

    constexpr std::array CUSTOM_INSTRUMENT_MULTIPLYING_FACTORS
    {
        0.5, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 10.0, 12.0, 12.0, 15.0, 15.0
    };

    constexpr std::array CUSTOM_INSTRUMENT_FEEDBACK_MODULATIONS
    {
        "0", "π/16", "π/8", "π/4", "π/2", "π", "2π", "4π"
    };

    constexpr std::array INSTRUMENT_NAMES
    {
        "User instrument", "Violin", "Guitar", "Piano", "Flute", "Clarinet", "Oboe", "Trumpet",
        "Organ", "Horn", "Synthesizer", "Harpsichord", "Vibraphone", "Synthesizer Bass", "Acoustic Bass", "Electric Guitar"
    };

    // Indices here correspond to bit indices
    constexpr std::array RHYTHM_INSTRUMENT_NAMES
    {
        "High hat", "Cymbal", "Tom-tom", "Snare drum", "Bass drum"
    };
}

YM2413State::YM2413State(const VgmHeader& header)
    : _clockRate(header.clock(Chip::YM2413)),
      _registers(0x39) { }

void YM2413State::add(const std::shared_ptr<const VgmCommands::ICommand>& command)
{
    const auto pCommand = std::dynamic_pointer_cast<const VgmCommands::YM2413>(command);
    if (!pCommand)
    {
        throw std::exception("Unhandled command type");
    }

    // We just stuff it in the registers (for now)
    if (VALID_REGISTERS.contains(pCommand->registerIndex()))
    {
        _registers[pCommand->registerIndex()] = pCommand->value();
    }
}

void YM2413State::copy_to_command_stream(CommandStream& stream, const std::shared_ptr<IChipState> lastWritten, bool fullImage) const
{
    const auto lastWrittenState = std::dynamic_pointer_cast<YM2413State>(lastWritten);
    // For most registers we just want to emit the value (if changed)
    for (uint8_t registerIndex = 0u; registerIndex < _registers.size(); ++registerIndex)
    {
        if (!VALID_REGISTERS.contains(registerIndex))
        {
            continue;
        }

        // TODO: key restarts, blips too
        if (fullImage || _registers[registerIndex] != lastWrittenState->_registers[registerIndex])
        {
            auto command = std::make_shared<VgmCommands::YM2413>();
            command->set_register(registerIndex);
            command->set_value(_registers[registerIndex]);
            stream.commands().push_back(command);
            lastWrittenState->_registers[registerIndex] = _registers[registerIndex];
        }
    }
}

std::shared_ptr<IChipState> YM2413State::clone() const
{
    return std::make_shared<YM2413State>(*this);
}

std::string YM2413State::percussion_instruments(const uint8_t value)
{
    std::ostringstream ss;
    bool isFirst = true;
    for (auto i = 4; i >= 0; --i)
    {
        if (Utils::bit_set(value, i))
        {
            if (isFirst)
            {
                isFirst = false;
            }
            else
            {
                ss << ", ";
            }
            ss << RHYTHM_INSTRUMENT_NAMES[i];
        }
    }
    return ss.str();
}

std::string YM2413State::percussion_volumes(const std::shared_ptr<const VgmCommands::YM2413>& pCommand)
{
    const auto volume1 = pCommand->value() >> 4;
    const auto volume2 = pCommand->value() & 0b1111;
    const auto attenuation1 = 3 * volume1;
    const auto attenuation2 = 3 * volume2;
    switch (pCommand->registerIndex())
    {
    case 0x36:
        return std::format("{} -> vol 0x{:x} = {:3} dB attenuation = {:3.0f}%",
            RHYTHM_INSTRUMENT_NAMES[4],
            volume2,
            attenuation2,
            Utils::db_to_percent(attenuation2));
    case 0x37:
        return std::format("{} -> vol 0x{:x} = {:3} dB attenuation = {:3.0f}%; {} -> vol 0x{:x} = {:3} dB attenuation = {:3.0f}%",
            RHYTHM_INSTRUMENT_NAMES[0],
            volume1,
            attenuation1,
            Utils::db_to_percent(attenuation1),
            RHYTHM_INSTRUMENT_NAMES[3],
            volume2,
            attenuation2,
            Utils::db_to_percent(attenuation2));
    case 0x38:
        return std::format("{} -> vol 0x{:x} = {:3} dB attenuation = {:3.0f}%; {} -> vol 0x{:x} = {:3} dB attenuation = {:3.0f}%",
            RHYTHM_INSTRUMENT_NAMES[2],
            volume1,
            attenuation1,
            Utils::db_to_percent(attenuation1),
            RHYTHM_INSTRUMENT_NAMES[1],
            volume2,
            attenuation2,
            Utils::db_to_percent(attenuation2));
    }
    throw std::runtime_error(std::format("Unexpected register index {}", pCommand->registerIndex()));
}

int YM2413State::f_number(const int channel) const
{
    return _registers[channel + 0x10] |
        ((_registers[channel + 0x20] & 1) << 8);
}

int YM2413State::block(const int channel) const
{
    return (_registers[channel + 0x20] & 0b1110) >> 1;
}

double YM2413State::frequency(const int channel) const
{
    return static_cast<double>(f_number(channel)) * _clockRate / 72 / (1 << (19 - block(channel)));
}

void YM2413State::to_text(const std::shared_ptr<const VgmCommands::ICommand>& pCommand, std::ostream& s)
{
    const auto& p = std::dynamic_pointer_cast<const VgmCommands::YM2413>(pCommand);
    if (p == nullptr)
    {
        throw std::runtime_error("Unexpected command type");
    }

    add(p);

    const auto registerIndex = p->registerIndex();
    const auto value = p->value();

    // Check if valid
    if (!VALID_REGISTERS.contains(p->registerIndex()))
    {
        s << "Invalid register index " << std::format("{:03x}", p->registerIndex());
        return;
    }

    // Check for custom instrument
    // ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
    switch (p->registerIndex())
    {
    case 0x00:
    case 0x01:
        s << "Tone user instrument ("
            << (registerIndex == 1 ? "carrier" : "modulator")
            << "): multiplier " << CUSTOM_INSTRUMENT_MULTIPLYING_FACTORS[value & 0b1111]
            << ", key scale rate " << Utils::bit_value(value, 4)
            << ", " << (Utils::bit_set(value, 5) ? "sustained" : "percussive") << " tone, vibrato "
            << Utils::on_off(value, 6)
            << ", AM " << Utils::on_off(value, 7);
        return;
    case 0x02:
        {
            const double keyScaleLevel = 1.5 * (value >> 6);
            const double attenuation = 0.75 * (value & 0b111111);
            s << "Tone user instrument: modulator key scale level " << keyScaleLevel << " dB/oct, total level "
                << attenuation << " dB = " << std::format("{:3.0f}%", Utils::db_to_percent(attenuation));
            return;
        }
    case 0x03:
        {
            const double keyScaleLevel = 1.5 * (value >> 6);
            s << "Tone user instrument: carrier key scale level " << keyScaleLevel << " db/oct"
                << ", carrier " << (Utils::bit_set(value, 4) ? "" : "not ") << "rectified"
                << ", modulator " << (Utils::bit_set(value, 3) ? "" : "not ") << "rectified"
                << ", feedback modulation " << CUSTOM_INSTRUMENT_FEEDBACK_MODULATIONS[value & 0b111];
            return;
        }
    case 0x04:
    case 0x05:
        {
            const int attackRate = value >> 4;
            const int decayRate = value & 0xf;
            s << "Tone user instrument (" << (p->registerIndex() == 4 ? "modulator" : "carrier") << "): "
                << "attack rate " << attackRate
                << ", decay rate " << decayRate;
            return;
        }
    case 0x06:
    case 0x07:
        {
            const int sustainLevel = 3 * (value >> 4);
            const int releaseRate = value & 0xf;
            s << "Tone user instrument (" << (p->registerIndex() == 6 ? "modulator" : "carrier") << "): "
                << "sustain level " << sustainLevel << " dB = " << std::format("{:3.0f}%", Utils::db_to_percent(sustainLevel))
                << ", release rate " << releaseRate;
            return;
        }
    case 0x0e: // Percussion
        s << "Rhythm control: percussion " << Utils::on_off(value, 5) << ", instruments: " << percussion_instruments(value);
        return;
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x18: // Tone F-number low 8 bits
        {
            const auto channel = p->registerIndex() & 0xf;
            const auto frequency = this->frequency(channel);
            s << "Tone frequency: ch " << channel << " -> " << std::format("{:03d}", f_number(channel))
                << "(" << block(channel) << ") = " << std::format("{:8.2f}", frequency) << " Hz = " << Utils::note_name(frequency);
            if (channel >= 6)
            {
                s << " OR Percussion F-num";
            }
            return;
        }
    case 0x20:
    case 0x21:
    case 0x22:
    case 0x23:
    case 0x24:
    case 0x25:
    case 0x26:
    case 0x27:
    case 0x28: // Tone F-number high bit, block, key, sustain
        {
            auto channel = p->registerIndex() & 0xf;
            auto frequency = this->frequency(channel);
            s << std::format("YM2413: Tone frequency/key: ch {} -> {:03d}({}) = {:8.2f} Hz = {}; sustain {}, key {}{}",
                channel,
                f_number(channel),
                block(channel),
                frequency,
                Utils::note_name(frequency),
                Utils::on_off(value, 5),
                Utils::on_off(value, 4),
                channel >= 6 ? " OR Percussion F-num" : "");
            return;
        }
    case 0x30:
    case 0x31:
    case 0x32:
    case 0x33:
    case 0x34:
    case 0x35:
    case 0x36:
    case 0x37:
    case 0x38: // Instrument/volumes
        {
            const auto channel = p->registerIndex() & 0xf;
            const auto instrument = value >> 4;
            const auto volume = value & 0b1111;
            const auto attenuation = 3 * volume;
            s << std::format("YM2413: Tone volume/instrument: ch {} -> vol 0x{:x} = {:3} dB attenuation = {:3.0f}%; inst 0x{:x} = {}{}",
                channel,
                volume,
                attenuation,
                Utils::db_to_percent(attenuation),
                instrument,
                INSTRUMENT_NAMES[instrument],
                channel < 6 ? "" : " OR Percussion volumes " + percussion_volumes(p));
            return;
        }
    }

    throw std::runtime_error("Unhandled value");
}

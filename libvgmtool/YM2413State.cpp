#include "YM2413State.h"

#include <format>
#include <sstream>
#include <unordered_set>

#include "utils.h"
#include "VgmHeader.h"
#include "VgmCommands.h"

namespace
{
    const std::unordered_set<int> validRegisters
    {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, // Custom instrument
        0x0e, // Rhythm control
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, // F-number low 8 bits
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, // F-number high bit, block, key, sustain
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, // Instrument, volume
    };

    const std::vector<double> customInstrumentMultiplyingFactors
    {
        0.5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 12, 12, 15, 15
    };

    const std::vector customInstrumentFeedbackModulations
    {
        "0", "π/16", "π/8", "π/4", "π/2", "π", "2π", "4π"
    };

    const std::vector instrumentNames
    {
        "User instrument", "Violin", "Guitar", "Piano", "Flute", "Clarinet", "Oboe", "Trumpet",
        "Organ", "Horn", "Synthesizer", "Harpsichord", "Vibraphone", "Synthesizer Bass", "Acoustic Bass", "Electric Guitar"
    };

    // Indices here correspond to bit indices
    const std::vector rhythmInstrumentNames
    {
        "High hat", "Cymbal", "Tom-tom", "Snare drum", "Bass drum"
    };
}

YM2413State::YM2413State(const VgmHeader& header)
    : _clockRate(header.clock(VgmHeader::Chip::YM2413)),
      _registers(0x39) { }

void YM2413State::add(const VgmCommands::YM2413* pCommand)
{
    // We just stuff it in the registers (for now)
    if (validRegisters.contains(pCommand->registerIndex()))
    {
        _registers[pCommand->registerIndex()] = pCommand->value();
    }
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
            ss << rhythmInstrumentNames[i];
        }
    }
    return ss.str();
}

std::string YM2413State::percussion_volumes(const VgmCommands::YM2413* pCommand)
{
    const auto volume1 = pCommand->value() >> 4;
    const auto volume2 = pCommand->value() & 0b1111;
    const auto attenuation1 = 3 * volume1;
    const auto attenuation2 = 3 * volume2;
    switch (pCommand->registerIndex())
    {
    case 0x36:
        return std::format("{} -> vol 0x{:x} = {:3} dB attenuation = {:3.0f}%",
            rhythmInstrumentNames[4],
            volume2,
            attenuation2,
            Utils::db_to_percent(attenuation2));
    case 0x37:
        return std::format("{} -> vol 0x{:x} = {:3} dB attenuation = {:3.0f}%; {} -> vol 0x{:x} = {:3} dB attenuation = {:3.0f}%",
            rhythmInstrumentNames[0],
            volume1,
            attenuation1,
            Utils::db_to_percent(attenuation1),
            rhythmInstrumentNames[3],
            volume2,
            attenuation2,
            Utils::db_to_percent(attenuation2));
    case 0x38:
        return std::format("{} -> vol 0x{:x} = {:3} dB attenuation = {:3.0f}%; {} -> vol 0x{:x} = {:3} dB attenuation = {:3.0f}%",
            rhythmInstrumentNames[2],
            volume1,
            attenuation1,
            Utils::db_to_percent(attenuation1),
            rhythmInstrumentNames[1],
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

std::string YM2413State::add_with_text(const VgmCommands::ICommand* pCommand)
{
    const auto* p = dynamic_cast<const VgmCommands::YM2413*>(pCommand);
    if (p == nullptr)
    {
        throw std::runtime_error("Unexpected command type");
    }

    add(p);

    const auto registerIndex = p->registerIndex();
    const auto value = p->value();

    // Check if valid
    if (!validRegisters.contains(p->registerIndex()))
    {
        return std::format("YM2413: Invalid register index {:03x}", p->registerIndex());
    }

    // Check for custom instrument
    // ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
    switch (p->registerIndex())
    {
    case 0x00:
    case 0x01:
        return std::format(
            "YM2413: Tone user instrument ({}): multiplier {}, key scale rate {}, {} tone, vibrato {}, AM {}",
            registerIndex == 1 ? "carrier" : "modulator",
            customInstrumentMultiplyingFactors[value & 0b1111],
            Utils::bit_value(value, 4),
            Utils::bit_set(value, 5) ? "sustained" : "percussive",
            Utils::on_off(value, 6),
            Utils::on_off(value, 7));
    case 0x02:
        {
            double keyScaleLevel = 1.5 * (value >> 6);
            double attenuation = 0.75 * (value & 0b111111);
            return std::format(
                "YM2413: Tone user instrument: modulator key scale level {} dB/oct, total level {} dB = {:3.0f}%",
                keyScaleLevel,
                attenuation,
                Utils::db_to_percent(attenuation));
        }
    case 0x03:
        return std::format(
            "YM2413: Tone user instrument: carrier key scale level {} db/oct, carrier {}rectified, modulator {}rectified, feedback modulation {}",
            1.5 * (value >> 6),
            Utils::bit_set(value, 4) ? "" : "not ",
            Utils::bit_set(value, 3) ? "" : "not ",
            customInstrumentFeedbackModulations[value & 0b111]);
    case 0x04:
    case 0x05:
        {
            int attackRate = value >> 4;
            int decayRate = value & 0xf;
            return std::format(
                "YM2413: Tone user instrument ({}): attack rate {}, decay rate {}",
                p->registerIndex() == 4 ? "modulator" : "carrier",
                attackRate,
                decayRate);
        }
    case 0x06:
    case 0x07:
        {
            int sustainLevel = 3 * (value >> 4);
            int releaseRate = value & 0xf;
            return std::format(
                "YM2413: Tone user instrument ({}): sustain level {} dB = {:3.0f}%, release rate {}",
                p->registerIndex() == 6 ? "modulator" : "carrier",
                sustainLevel,
                Utils::db_to_percent(sustainLevel),
                releaseRate);
        }
    case 0x0e: // Percussion
        return std::format(
            "YM2413: Rhythm control: percussion {}, instruments: {}",
            Utils::on_off(value, 5),
            percussion_instruments(value));
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
            auto channel = p->registerIndex() & 0xf;
            auto frequency = this->frequency(channel);
            return std::format("YM2413: Tone frequency: ch {} -> {:03d}({}) = {:8.2f} Hz = {}{}",
                channel,
                f_number(channel),
                block(channel),
                frequency,
                Utils::note_name(frequency),
                channel >= 6 ? " OR Percussion F-num" : "");
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
            return std::format("YM2413: Tone frequency/key: ch {} -> {:03d}({}) = {:8.2f} Hz = {}; sustain {}, key {}{}",
                channel,
                f_number(channel),
                block(channel),
                frequency,
                Utils::note_name(frequency),
                Utils::on_off(value, 5),
                Utils::on_off(value, 4),
                channel >= 6 ? " OR Percussion F-num" : "");
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
            return std::format("YM2413: Tone volume/instrument: ch {} -> vol 0x{:x} = {:3} dB attenuation = {:3.0f}%; inst 0x{:x} = {}{}",
                channel,
                volume,
                attenuation,
                Utils::db_to_percent(attenuation),
                instrument,
                instrumentNames[instrument],
                channel < 6 ? "" : " OR Percussion volumes " + percussion_volumes(p));
        }
    }

    throw std::runtime_error("Unhandled value");
}

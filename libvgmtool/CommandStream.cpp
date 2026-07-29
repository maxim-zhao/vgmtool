#include "CommandStream.h"

#include <format>
#include <stdexcept>

#include "vgm.h"

void CommandStream::from_data(BinaryData& data, uint32_t endOffset, bool expectEnd)
{
    register_commands();

    while (data.offset() < endOffset && data.offset() < data.size())
    {
        const auto marker = data.peek();
        auto it = _commandGenerators.find(marker);
        if (it == _commandGenerators.end())
        {
            throw std::runtime_error(std::format("No generator for marker {:x}", marker));
        }
        auto pCommand = it->second(data);
        _commands.push_back(pCommand);

        if (std::dynamic_pointer_cast<const VgmCommands::End>(pCommand))
        {
            if (data.offset() != endOffset)
            {
                throw std::runtime_error(std::format(
                    "End of VGM data at offset {:x}, {} bytes unaccounted for before expected end at {:x}",
                    data.offset() - 1,
                    endOffset - data.offset(),
                    endOffset));
            }
            return;
        }
    }
    if (expectEnd)
    {
        // If we get there then we ran out of data before we saw EOF
        throw std::runtime_error("No EOF marker found in VGM data");
    }
}

void CommandStream::to_binary(BinaryData& data) const
{
    for (const auto& pData : _commands)
    {
        pData->to_data(data);
    }
}

void CommandStream::add_pause(int pauseLength)
{
    if (pauseLength == 0)
    {
        return;
    }

    // This is not quite optimal - it depends upon what the length modulo 0xffff is.
    // If it is not any of the <3 byte options, we would be better off emitting a
    // 16-bit wait that makes it so, if possible. This is unlikely to happen
    // very often.

    while (pauseLength > 0xffff)
    {
        const auto wait = std::make_shared<VgmCommands::Wait16bit>();
        wait->set_duration(0xffff);
        _commands.push_back(wait);
        pauseLength -= 0xffff;
    }

    // Two one-byte commands are more efficient than a three-byte wait
    if (pauseLength == LEN60TH * 2)
    {
        _commands.push_back(std::make_shared<VgmCommands::Wait60th>());
        _commands.push_back(std::make_shared<VgmCommands::Wait60th>());
    }
    else if (pauseLength == LEN60TH)
    {
        _commands.push_back(std::make_shared<VgmCommands::Wait60th>());
    }
    else if (pauseLength == LEN50TH * 2)
    {
        _commands.push_back(std::make_shared<VgmCommands::Wait50th>());
        _commands.push_back(std::make_shared<VgmCommands::Wait50th>());
    }
    else if (pauseLength == LEN50TH)
    {
        _commands.push_back(std::make_shared<VgmCommands::Wait50th>());
    }
    else if (pauseLength <= 16)
    {
        const auto wait = std::make_shared<VgmCommands::Wait4Bit>();
        wait->set_duration(pauseLength);
        _commands.push_back(wait);
    }
    else
    {
        const auto wait = std::make_shared<VgmCommands::Wait16bit>();
        wait->set_duration(static_cast<uint16_t>(pauseLength));
        _commands.push_back(wait);
    }
}

void CommandStream::register_commands()
{
    if (!_commandGenerators.empty())
    {
        return;
    }
    // Register all the commands we have handlers for
    // 0x00 to 0x2f: undefined
    register_command<VgmCommands::InvalidCommand>(0x00, 0x2f);
    // 0x30 to 0x3f: reserved, one byte
    register_command<VgmCommands::ReservedCommand<1>>(0x30, 0x3f);
    // 0x40 to 0x4e: reserved, two bytes
    register_command<VgmCommands::ReservedCommand<2>>(0x40, 0x4e);
    // 0x4f: GG stereo
    register_command<VgmCommands::GGStereo>();
    // 0x50-0x5f: chip commands
    register_command<VgmCommands::SN76489>();
    register_command<VgmCommands::YM2413>();
    register_command<VgmCommands::YM2612Port0>();
    register_command<VgmCommands::YM2612Port1>();
    register_command<VgmCommands::YM2151>();
    register_command<VgmCommands::YM2203>();
    register_command<VgmCommands::YM2608Port0>();
    register_command<VgmCommands::YM2608Port1>();
    register_command<VgmCommands::YM2610Port0>();
    register_command<VgmCommands::YM2610Port1>();
    register_command<VgmCommands::YM3812>();
    register_command<VgmCommands::YM3526>();
    register_command<VgmCommands::Y8950>();
    register_command<VgmCommands::YMZ280B>();
    register_command<VgmCommands::YMF262Port0>();
    register_command<VgmCommands::YMF262Port1>();
    // 0x60 undefined
    register_command<VgmCommands::InvalidCommand>(0x60, 0x60);
    // 0x61-0x63: wait commands
    register_command<VgmCommands::Wait16bit>();
    register_command<VgmCommands::Wait60th>();
    register_command<VgmCommands::Wait50th>();
    // 0x64, 0x65 undefined
    // 0x66-0x68: end, data blocks, PCM writes
    register_command<VgmCommands::End>();
    register_command<VgmCommands::DataBlock>();
    register_command<VgmCommands::PcmRamWrite>();
    // 0x69 to 0x6f undefined
    // 0x70-0x7f: 4-bit waits
    register_command<VgmCommands::Wait4Bit>(0x70, 0x7f);
    // 0x80-0x8f: YM2612 samples with waits
    register_command<VgmCommands::YM2612Sample>(0x80, 0x8f);
    // 0x90-0x95: DAC stream control
    register_command<VgmCommands::DacStreamControlSetup>();
    // 0xa0-0xaf: AY8910, and "second" YM chips
    register_command<VgmCommands::AY8910>();
    register_command<VgmCommands::YM2413_Second>();
    register_command<VgmCommands::YM2612Port0_Second>();
    register_command<VgmCommands::YM2612Port1_Second>();
    register_command<VgmCommands::YM2151_Second>();
    register_command<VgmCommands::YM2203_Second>();
    register_command<VgmCommands::YM2608Port0_Second>();
    register_command<VgmCommands::YM2608Port1_Second>();
    register_command<VgmCommands::YM2610Port0_Second>();
    register_command<VgmCommands::YM2610Port1_Second>();
    register_command<VgmCommands::YM3812_Second>();
    register_command<VgmCommands::YM3526_Second>();
    register_command<VgmCommands::Y8950_Second>();
    register_command<VgmCommands::YMZ280B_Second>();
    register_command<VgmCommands::YMF262Port0_Second>();
    register_command<VgmCommands::YMF262Port1_Second>();
    // 0xb0-0xbf: 2-byte commands
    register_command<VgmCommands::RF5C68Register>();
    register_command<VgmCommands::RF5C164Register>();
    register_command<VgmCommands::PWM>();
    register_command<VgmCommands::ReservedCommand<2>>(0xb3, 0xbf);
    // 0xc0-0xcf: 3-byte commands
    register_command<VgmCommands::SegaPCM>();
    register_command<VgmCommands::RF5C68Memory>();
    register_command<VgmCommands::RF5C164Memory>();
    register_command<VgmCommands::ReservedCommand<3>>(0xc3, 0xcf);
    // 0xd0-0xdf: 3-byte commands
    register_command<VgmCommands::YMF278B>();
    register_command<VgmCommands::YMF271>();
    register_command<VgmCommands::ReservedCommand<3>>(0xd2, 0xdf);
    // 0xe0-0xef: 4-byte commands
    register_command<VgmCommands::PCMSeek>();
    register_command<VgmCommands::ReservedCommand<4>>(0xe1, 0xef);

    // Then register everything else
    // - Some ranges are reserved
    // - Some ranges are undefined
    // TODO
    /*
    for (int i = 0; i < 256; ++i)
    {
        const auto marker = static_cast<uint8_t>(i);
        if (_commandGenerators.contains(marker))
        {
            continue;
        }
    }
    */
}

template <typename T>
void CommandStream::register_command()
{
    // We make one temporarily in order to get its marker
    T temp;
    const auto marker = temp.get_marker();
    if (_commandGenerators.contains(marker))
    {
        throw std::runtime_error(std::format("Registering type {:x} for a second time", marker));
    }
    _commandGenerators.insert(std::make_pair(marker, [&](BinaryData& data)
    {
        auto t = std::make_shared<T>();
        t->from_data(data);
        return std::move(t);
    }));
}

template <typename T>
void CommandStream::register_command(const uint8_t min, const uint8_t max)
{
    for (auto marker = min; marker <= max; ++marker)
    {
        if (_commandGenerators.contains(marker))
        {
            throw std::runtime_error(std::format("Registering type {:x} for a second time", marker));
        }
        _commandGenerators.insert(std::make_pair(marker, [&](BinaryData& data)
        {
            // These command types have to have no-parameter constructors
            auto t = std::make_shared<T>();
            // ...and consume the marker in here
            t->from_data(data);
            return std::move(t);
        }));
    }
}

void CommandStream::optimise_pauses()
{
    // We walk the command stream, merging any consecutive pure pauses.
    // We do this by copying the non-pauses into a new object as we go, then swapping.
    auto currentPauseLength = 0;
    CommandStream temp;
    for (const auto& command : _commands)
    {
        if (const auto& pause = std::dynamic_pointer_cast<const VgmCommands::Wait>(command);
            pause && command->chip() == Chip::Nothing)
        {
            // It's a pause. Add to the running total.
            currentPauseLength += pause->duration();
        }
        else
        {
            // Emit any pending pause
            if (currentPauseLength > 0)
            {
                temp.add_pause(currentPauseLength);
                currentPauseLength = 0;
            }
            temp._commands.push_back(command);
        }
    }
    // And any trailing pause
    if (currentPauseLength > 0)
    {
        temp.add_pause(currentPauseLength);
    }
    // Finally, swap it in
    _commands.swap(temp._commands);
}

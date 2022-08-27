#include "CommandStream.h"

#include <format>
#include <ranges>
#include <stdexcept>

CommandStream::CommandStream()
{
    // Register all the commands we have handlers for
    // 0x30 to 0x4e: reserved
    register_command<VgmCommands::ReservedCommand<1>>(0x30, 0x4e);
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
    register_command<VgmCommands::Wait4bit>(0x70, 0x7f);
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

void CommandStream::from_data(BinaryData& data, uint32_t loop_offset, uint32_t end_offset)
{
    // TODO check for end of data?
    while (data.offset() < end_offset)
    {
        // We inject a "loop point" virtual command here.
        if (data.offset() == loop_offset)
        {
            _data.push_back(new VgmCommands::LoopPoint());
        }

        const auto marker = data.peek();
        auto it = _commandGenerators.find(marker);
        if (it == _commandGenerators.end())
        {
            throw std::runtime_error(std::format("No generator for marker {:x}", marker));
        }
        auto pCommand = it->second(data);
        _data.push_back(pCommand);

        if (dynamic_cast<VgmCommands::End*>(pCommand) != nullptr)
        {
            if (data.offset() != end_offset)
            {
                throw std::runtime_error(std::format(
                    "End of VGM data at offset {:x}, {} bytes unaccounted for before expected end at {:x}",
                    data.offset() - 1,
                    end_offset - data.offset(),
                    end_offset));
            }
            return;
        }
    }
    // If we get there then we ran out of data before we saw EOF
    throw std::runtime_error("No EOF marker found in VGM data");
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
        T* t = new T();
        t->from_data(data);
        return t;
    }));
}

template <typename T>
void CommandStream::register_command(uint8_t min, uint8_t max)
{
    for (auto marker = min; marker <= max; ++marker)
    {
        if (_commandGenerators.contains(marker))
        {
            throw std::runtime_error(std::format("Registering type {:x} for a second time", marker));
        }
        _commandGenerators.insert(std::make_pair(marker, [&](BinaryData& data)
        {
            T* t = new T();
            t->from_data(data);
            return t;
        }));
    }
}

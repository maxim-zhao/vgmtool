#include "CommandStream.h"

#include <format>
#include <ranges>
#include <stdexcept>

CommandStream::CommandStream()
{
    // Register all the commands we have handlers for
    register_command<VgmCommands::GGStereo>();
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
    register_command<VgmCommands::Wait>();
    register_command<VgmCommands::Wait60th>();
    register_command<VgmCommands::Wait50th>();
    // 0x64, 0x65 undefined
    register_command<VgmCommands::End>();
    register_command<VgmCommands::DataBlock>();
    register_command<VgmCommands::PcmRamWrite>();
    // 0x69 to 0x6f undefined
    register_command<VgmCommands::WaitN<0x70>>();
    register_command<VgmCommands::WaitN<0x71>>();
    register_command<VgmCommands::WaitN<0x72>>();
    register_command<VgmCommands::WaitN<0x73>>();
    register_command<VgmCommands::WaitN<0x74>>();
    register_command<VgmCommands::WaitN<0x75>>();
    register_command<VgmCommands::WaitN<0x76>>();
    register_command<VgmCommands::WaitN<0x77>>();
    register_command<VgmCommands::WaitN<0x78>>();
    register_command<VgmCommands::WaitN<0x79>>();
    register_command<VgmCommands::WaitN<0x7a>>();
    register_command<VgmCommands::WaitN<0x7b>>();
    register_command<VgmCommands::WaitN<0x7c>>();
    register_command<VgmCommands::WaitN<0x7d>>();
    register_command<VgmCommands::WaitN<0x7e>>();
    register_command<VgmCommands::WaitN<0x7f>>();
    register_command<VgmCommands::YM2612Sample<0x80>>();
    register_command<VgmCommands::YM2612Sample<0x81>>();
    register_command<VgmCommands::YM2612Sample<0x82>>();
    register_command<VgmCommands::YM2612Sample<0x83>>();
    register_command<VgmCommands::YM2612Sample<0x84>>();
    register_command<VgmCommands::YM2612Sample<0x85>>();
    register_command<VgmCommands::YM2612Sample<0x86>>();
    register_command<VgmCommands::YM2612Sample<0x87>>();
    register_command<VgmCommands::YM2612Sample<0x88>>();
    register_command<VgmCommands::YM2612Sample<0x89>>();
    register_command<VgmCommands::YM2612Sample<0x8a>>();
    register_command<VgmCommands::YM2612Sample<0x8b>>();
    register_command<VgmCommands::YM2612Sample<0x8c>>();
    register_command<VgmCommands::YM2612Sample<0x8d>>();
    register_command<VgmCommands::YM2612Sample<0x8e>>();
    register_command<VgmCommands::YM2612Sample<0x8f>>();

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

        const auto marker = data.read_uint8();
        auto it = _commandGenerators.find(marker);
        if (it == _commandGenerators.end())
        {
            throw std::runtime_error(std::format("No generator for marker {:x}", marker));
        }
        auto pCommand = it->second(data);
        _data.push_back(pCommand);

        if (marker == VgmCommands::End::get_marker_static())
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
    _commandGenerators.insert(std::make_pair(T::get_marker_static(), [&](BinaryData& data)
    {
        return new T(data);
    }));
}

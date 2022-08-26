#include "CommandStream.h"

#include <format>
#include <stdexcept>

CommandStream::CommandStream()
{
    // Register all the commands
    register_command<VgmCommands::GGStereo>();
    register_command<VgmCommands::SN76489>();
    register_command<VgmCommands::YM2413>();
    register_command<VgmCommands::YM2612Port0>();
    register_command<VgmCommands::YM2612Port1>();
    register_command<VgmCommands::YM2151>();
    register_command<VgmCommands::Wait>();
    register_command<VgmCommands::Wait60th>();
    register_command<VgmCommands::Wait50th>();
    register_command<VgmCommands::End>();
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

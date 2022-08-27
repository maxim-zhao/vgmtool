#pragma once
#include <cstdint>
#include <functional>
#include <vector>

#include "VgmCommands.h"

class CommandStream
{
public:
    CommandStream();
    // explicit CommandStream(BinaryData& data);

    void from_data(BinaryData& data, uint32_t loop_offset, uint32_t end_offset);

    std::vector<VgmCommands::ICommand*>& commands()
    {
        return _data;
    }

private:
    template <typename T>
    void register_command();
    template <typename T>
    void register_command(uint8_t min, uint8_t max);

    std::vector<VgmCommands::ICommand*> _data;
    std::unordered_map<uint8_t, std::function<VgmCommands::ICommand*(BinaryData& data)>> _commandGenerators;
};

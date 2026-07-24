#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "VgmCommands.h"

class CommandStream
{
public:
    void from_data(BinaryData& data, uint32_t endOffset);

    std::vector<std::shared_ptr<VgmCommands::ICommand>>& commands()
    {
        return _commands;
    }

    [[nodiscard]] const std::vector<std::shared_ptr<VgmCommands::ICommand>>& commands() const
    {
        return _commands;
    }

    void to_binary(BinaryData& data) const;

    void optimise_pauses();
    void add_pause(int length);

private:
    void register_commands();
    template <typename T>
    void register_command();
    template <typename T>
    void register_command(uint8_t min, uint8_t max);

    std::vector<std::shared_ptr<VgmCommands::ICommand>> _commands;
    std::unordered_map<uint8_t, std::function<std::shared_ptr<VgmCommands::ICommand>(BinaryData& data)>> _commandGenerators;
};

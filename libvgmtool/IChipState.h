#pragma once
#include <memory>

#include "VgmCommands.h"

class CommandStream;

class IChipState
{
public:
    virtual ~IChipState() = default;
    virtual void add(const std::shared_ptr<const VgmCommands::ICommand>& command) = 0;
    virtual void copy_to_command_stream(CommandStream& stream, std::shared_ptr<IChipState> lastWritten, bool fullImage) const = 0;
    [[nodiscard]] virtual std::shared_ptr<IChipState> clone() const = 0;
};

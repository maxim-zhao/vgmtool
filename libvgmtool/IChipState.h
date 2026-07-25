#pragma once
#include <memory>

#include "VgmCommands.h"

class CommandStream;

class IChipState
{
public:
    enum class WriteTypes: std::uint8_t
    {
        force_full_image, // Publish a full image
        force_delta, // Publish a delta from lastWritten
        automatic // Let the chip state publish as it wishes: a delta or something else
    };

    virtual ~IChipState() = default;
    virtual void add(const std::shared_ptr<const VgmCommands::ICommand>& command) = 0;
    virtual void copy_to_command_stream(CommandStream& stream, std::shared_ptr<IChipState> lastWritten, WriteTypes mode) = 0;
    [[nodiscard]] virtual std::shared_ptr<IChipState> clone() const = 0;
};

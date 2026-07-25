#pragma once
#include <vector>
#include <memory>
#include <string>

#include "IChipState.h"
#include "VgmCommands.h"

namespace VgmCommands
{
    class ICommand;
}

class VgmHeader;

class YM2413State: public IChipState
{
public:
    explicit YM2413State(const VgmHeader& header);

    void to_text(const std::shared_ptr<const VgmCommands::ICommand>& pCommand, std::ostream& s);

    void add(const std::shared_ptr<const VgmCommands::ICommand>& command) override;
    void copy_to_command_stream(CommandStream& stream, std::shared_ptr<IChipState> lastWritten, WriteTypes mode) override;
    [[nodiscard]] std::shared_ptr<IChipState> clone() const override;

private:
    static std::string percussion_instruments(uint8_t value);
    static std::string percussion_volumes(const std::shared_ptr<const VgmCommands::YM2413>& pCommand);
    [[nodiscard]] int f_number(int channel) const;
    [[nodiscard]] int block(int channel) const;
    [[nodiscard]] double frequency(int channel) const;

    uint32_t _clockRate;
    std::vector<uint8_t> _registers;

    // This holds a queue of register writes since the last flush
    std::vector<std::shared_ptr<const VgmCommands::YM2413>> _eventsQueue;
};

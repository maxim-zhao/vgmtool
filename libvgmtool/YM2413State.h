#pragma once
#include <memory>
#include <string>

#include "VgmCommands.h"

namespace VgmCommands
{
    class ICommand;
}

class VgmHeader;

class YM2413State
{
public:
    explicit YM2413State(const VgmHeader& header);

    void to_text(const std::shared_ptr<const VgmCommands::ICommand>& pCommand, std::ostream& s);

    void add(const std::shared_ptr<const VgmCommands::YM2413>& pCommand);

private:
    static std::string percussion_instruments(uint8_t value);
    static std::string percussion_volumes(const std::shared_ptr<const VgmCommands::YM2413>& pCommand);
    [[nodiscard]] int f_number(int channel) const;
    [[nodiscard]] int block(int channel) const;
    [[nodiscard]] double frequency(int channel) const;

    uint32_t _clockRate;
    std::vector<uint8_t> _registers;
};

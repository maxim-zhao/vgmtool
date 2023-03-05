#pragma once
#include <string>
#include <unordered_map>

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

    void add_with_text(const VgmCommands::ICommand* pCommand, std::ostream& s);

    void add(const VgmCommands::YM2413* pCommand);

private:
    static std::string percussion_instruments(uint8_t value);
    static std::string percussion_volumes(const VgmCommands::YM2413* pCommand);
    [[nodiscard]] int f_number(int channel) const;
    [[nodiscard]] int block(int channel) const;
    [[nodiscard]] double frequency(int channel) const;

    uint32_t _clockRate;
    std::vector<uint8_t> _registers;
};

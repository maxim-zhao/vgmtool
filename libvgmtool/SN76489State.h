#pragma once

#include <memory>
#include <string>
#include <vector>

#include "IChipState.h"

class CommandStream;

namespace VgmCommands
{
    class ICommand;
    class SN76489;
    class GGStereo;
}

class VgmHeader;

class SN76489State: public IChipState
{
public:
    explicit SN76489State(const VgmHeader& header);

    void add(const std::shared_ptr<const VgmCommands::GGStereo>& pStereo);
    void add(const std::shared_ptr<const VgmCommands::SN76489>& pCommand);
    void to_text(std::ostream& s, const std::shared_ptr<const VgmCommands::ICommand>& pCommand);
    void copy_to_command_stream(CommandStream& stream, SN76489State& lastWrittenPsgState, bool fullImage) const;

private:
    void prepare_text();

    // Registers are four tone, volume pairs
    std::vector<int> _registers{0, 0xf, 0, 0xf, 0, 0xf, 0, 0xf};
    std::size_t _latchedRegisterIndex = 0;
    uint32_t _clockRate;
    uint8_t _stereoMask = 0xff;

    // To-text reusable text
    std::vector<std::string> _noiseSpeedDescriptions;
    std::vector<std::string> _volumeDescriptions;
};

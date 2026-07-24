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
    ~SN76489State() override = default;

    SN76489State(const SN76489State& other) = default;
    SN76489State(SN76489State&& other) noexcept = default;
    SN76489State& operator=(const SN76489State& other) = default;
    SN76489State& operator=(SN76489State&& other) noexcept = default;

    void add(const std::shared_ptr<const VgmCommands::ICommand>& command) override;
    void to_text(std::ostream& s, const std::shared_ptr<const VgmCommands::ICommand>& pCommand);
    void copy_to_command_stream(CommandStream& stream, std::shared_ptr<IChipState> lastWritten, bool fullImage) const override;
    [[nodiscard]] std::shared_ptr<IChipState> clone() const override;

private:
    void prepare_text();

private:
    // Registers are four tone, volume pairs
    std::vector<int> _registers{0, 0xf, 0, 0xf, 0, 0xf, 0, 0xf};
    std::size_t _latchedRegisterIndex = 0;
    uint32_t _clockRate;
    uint8_t _stereoMask = 0xff;

    // To-text reusable text
    std::vector<std::string> _noiseSpeedDescriptions;
    std::vector<std::string> _volumeDescriptions;
};

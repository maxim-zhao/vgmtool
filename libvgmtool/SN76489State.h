#pragma once

#include <string>
#include <vector>

namespace VgmCommands
{
    class ICommand;
    class SN76489;
    class GGStereo;
}

class VgmHeader;

class SN76489State
{
public:
    explicit SN76489State(const VgmHeader& header);

    void add(const VgmCommands::GGStereo* pStereo);
    void add(const VgmCommands::SN76489* pCommand);
    void add_with_text(const VgmCommands::ICommand* pCommand, std::ostream& s);

private:
    static std::string print_stereo_mask(uint8_t mask);
    [[nodiscard]] double tone_length_to_hz(int length) const;
    std::string make_noise_description(const char* prefix, int shift) const;

    // Registers are four tone, volume pairs
    std::vector<int> _registers{0, 0xf, 0, 0xf, 0, 0xf, 0, 0xf};
    uint8_t _stereoMask = 0xff;
    int _latchedRegisterIndex = 0;

    uint32_t _clockRate;
    std::vector<std::string> _noiseSpeedDescriptions;
    std::vector<std::string> _volumeDescriptions;
};

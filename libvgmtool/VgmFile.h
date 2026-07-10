#pragma once
#include <string>

#include "CommandStream.h"
#include "Gd3Tag.h"
#include "VgmHeader.h"


class YM2413State;
class SN76489State;
class IVGMToolCallback;

class VgmFile
{
    VgmHeader _header;
    CommandStream _dataBeforeLoop;
    CommandStream _dataWithLoop;
    Gd3Tag _gd3Tag;

public:
    VgmFile() = default;
    explicit VgmFile(const std::string& filename);

    void load_file(const std::string& filename);
    void save_file(const std::string& filename);

    VgmHeader& header()
    {
        return _header;
    }

    Gd3Tag& gd3()
    {
        return _gd3Tag;
    }

    // Checks the header. Throws on any errors found if fix=false, else tries to fix them.
    void check_header(bool fix);

    // Writes the VGM file as text to the stream
    void write_to_text(std::ostream& s, const IVGMToolCallback& callback) const;

private:
    static void write_command(
        std::ostream& s, 
        size_t& offset, 
        int& time, 
        SN76489State& psgState, 
        YM2413State& ym2413State,
        const VgmCommands::ICommand* pCommand);
};

#pragma once
#include <string>

#include "CommandStream.h"
#include "Gd3Tag.h"
#include "VgmHeader.h"


class YM2413State;
class SN76489State;
class IStatusCallback;

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
    void save_file(const std::string& filename, const IStatusCallback& callback, bool verbose_zopfli = false, int compression = 0);

    [[nodiscard]] VgmHeader& header()
    {
        return _header;
    }

    [[nodiscard]] const VgmHeader& header() const
    {
        return _header;
    }

    [[nodiscard]] Gd3Tag& gd3()
    {
        return _gd3Tag;
    }

    [[nodiscard]] const Gd3Tag& gd3() const
    {
        return _gd3Tag;
    }

    [[nodiscard]] CommandStream& data_before_loop()
    {
        return _dataBeforeLoop;
    }

    [[nodiscard]] CommandStream& data_with_loop()
    {
        return _dataWithLoop;
    }

    // Checks the header. Throws on any errors found if fix=false, else tries to fix them.
    void check_header(bool fix, const IStatusCallback& callback);

    // Writes the VGM file as text to the stream
    void write_to_text(std::ostream& s, const IStatusCallback& callback) const;

private:
    static void write_command_as_text(
        std::ostream& s,
        size_t& offset,
        int& time,
        SN76489State& psgState,
        YM2413State& ym2413State,
        const std::shared_ptr<const VgmCommands::ICommand>& pCommand);
};

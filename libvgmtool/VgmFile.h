#pragma once
#include <string>

#include "CommandStream.h"
#include "Gd3Tag.h"
#include "VgmHeader.h"

class IVGMToolCallback;

class VgmFile
{
    VgmHeader _header;
    CommandStream _data;
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

    void write_to_text(std::ostream& s, const IVGMToolCallback& callback) const;
};

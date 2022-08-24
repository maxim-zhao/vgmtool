#pragma once
#include <string>

#include "CommandStream.h"
#include "Gd3Tag.h"
#include "VgmHeader.h"


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

    // Checks the header. Throws on any errors found.
    void check_header();
};

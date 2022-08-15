#pragma once
#include <string>

#include "Gd3Tag.h"
#include "VgmHeader.h"

class VgmFile
{
    VgmHeader _header;
    Gd3Tag _gd3Tag;
    std::vector<uint8_t> _data;

public:
    explicit VgmFile(const std::string& filename);

    VgmHeader& header()
    {
        return _header;
    }

    Gd3Tag& gd3()
    {
        return _gd3Tag;
    }
};

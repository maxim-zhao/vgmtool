#pragma once
#include <string>

#include "vgm.h"

// Conversion routines

class IVGMToolCallback;

class Convert
{
public:
    static bool to_vgm(const std::string& filename, const IVGMToolCallback& callback);
private:
    static void gymToVgm(const std::string& filename, gzFile in,
                         gzFile out, VGMHeader& vgmHeader);
};

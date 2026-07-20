#pragma once
#include <string>

#include "vgm.h"

// Conversion routines

class IStatusCallback;

class Convert
{
public:
    static bool to_vgm(const std::string& filename, const IStatusCallback& callback);
private:
    static void gymToVgm(const std::string& filename, gzFile in,
                         gzFile out, OldVGMHeader& vgmHeader);
};

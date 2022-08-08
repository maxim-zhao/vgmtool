#pragma once
#include <string>

// Conversion routines

class IVGMToolCallback;

class Convert
{
public:
    enum class file_type { gym, ssl, cym };

    static bool to_vgm(const std::string& filename, file_type fileType, const IVGMToolCallback& callback);
};

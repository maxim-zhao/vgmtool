#pragma once
#include <cstdint>
#include <string>

// GD3 tag format definitions
// and functionality

class IVGMToolCallback;

// GD3 file header
struct TGD3Header
{
    char id_string[4]; // "Gd3 "
    uint32_t version; // 0x000000100 for 1.00
    uint32_t length; // Length of string data following this point
};

enum Gd3Indices
{
    TitleEn,
    TitleJp,
    GameEn,
    GameJp,
    SystemEn,
    SystemJp,
    AuthorEn,
    AuthorJp,
    ReleaseDate,
    Creator,
    Notes,
    Count
};

void remove_gd3(const std::string& filename, const IVGMToolCallback& callback);

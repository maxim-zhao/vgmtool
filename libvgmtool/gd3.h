#pragma once

// GD3 tag format definitions
// and functionality

class IVGMToolCallback;

// GD3 file header
struct TGD3Header
{
    char id_string[4]; // "Gd3 "
    long version; // 0x000000100 for 1.00
    long length; // Length of string data following this point
};

#define NumGD3Strings 11

void remove_gd3(char* filename, const IVGMToolCallback& callback);

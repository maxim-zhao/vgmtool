#pragma once

// General purpose functions, not specific to anything much
// except most are VGM-centric

#include <string>
#include <vector>

class IVGMToolCallback;

class Utils
{
public:
    // printf() for std::strings
#if defined(__RESHARPER__) || defined(__GNUC__)
    [[gnu::format(printf, 1, 2)]]
#endif
    static std::string format(const char* format, ...);

    // Returns true if filename exists
    static bool file_exists(const std::string& filename);
    // Returns the size of filename in bytes
    static int file_size(const std::string& filename);
    // Compresses filename in place with zopfli
    static void compress(const std::string& filename, int iterations = -1);
    // Decompresses filename in place
    static void decompress(const std::string& filename);
    // Reads a file into RAM, possibly decompressing it at the same time
    static void load_file(std::vector<uint8_t>& buffer, const std::string& filename);
    // Makes a temp filename (that is not in use) in the same directory as src and returns it. This is subject to a race but probably fine.
    static std::string make_temp_filename(const std::string& src);
    // Makes a filename by adding " (<suffix>)" to src before the extension
    static std::string make_suffixed_filename(const std::string& src, const std::string& suffix);
    // Deletes destination if it exists, and moves source to its place
    static void replace_file(const std::string& destination, const std::string& source);

    // Converts LSB b1, MSB b2 into an integer
    static int make_word(int b1, int b2);
};

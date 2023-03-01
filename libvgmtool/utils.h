#pragma once

// General purpose functions, not specific to anything much
// except most are VGM-centric

#include <string>
#include <vector>

#include "VgmCommands.h"

class IVGMToolCallback;

class Utils
{
public:
    // Returns s in lowercase
    static std::string to_lower(const std::string& s);

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

    // Prints a 44100Hz sample count as time in m:ss[.fff] form
    static std::string samples_to_display_text(uint32_t samples, bool withMilliseconds = false);

    // Converts a frequency in Hz to a standard MIDI note representation
    static std::string note_name(double frequencyHz);

    // Bit index as "on" (1) or "off" (0)
    static const std::string& on_off(uint8_t value, int bitIndex);

    // Bit index as 1 or 0
    static int bit_value(uint8_t value, int bitIndex);

    // Bit index as boolean
    static bool bit_set(uint8_t value, int bitIndex);

    // dB attenuation to percentage; 0 dB -> 100%, more dB -> less percent
    static double db_to_percent(double attenuation);
};

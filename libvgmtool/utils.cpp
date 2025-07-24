#include <cstdlib>
#include <zlib.h>
#include <filesystem>
#include "utils.h"

#include <fstream>
#include <stdexcept>
#include <utility>
#include <vector>
#include <zopfli.h>

#include "IVGMToolCallback.h"

bool Utils::file_exists(const std::string& filename)
{
    return std::filesystem::exists(filename);
}

// returns the size of the file in bytes
int Utils::file_size(const std::string& filename)
{
    return static_cast<int>(std::filesystem::file_size(filename));
}

void Utils::compress(const std::string& filename, const IVGMToolCallback& callback, const int iterations)
{
    auto sizeBefore = file_size(filename);

    // Read file into memory
    std::vector<uint8_t> data;
    load_file(data, filename);

    callback.show_status(std::format(
        "{} before: {} bytes ({} bytes uncompressed, {:.4}% compression)", 
        filename, 
        sizeBefore, 
        data.size(), 
        percentReduction(static_cast<int>(data.size()), sizeBefore)));

    // Now compress
    ZopfliOptions options{};
    ZopfliInitOptions(&options);
    if (iterations > 0)
    {
        // We let the library pick the default (15) if not set
        options.numiterations = iterations;
    }
    unsigned char* out;
    size_t outSize = 0;
    ZopfliCompress(&options, ZOPFLI_FORMAT_GZIP, data.data(), data.size(), &out, &outSize);

    // If it is not smaller, do not save
    if (std::cmp_greater_equal(outSize, sizeBefore))
    {
        callback.show_status(std::format(
            "Compressed to {} bytes, not overwriting...",
            outSize));
        return;
    }

    // Write to disk, over the original file
    std::ofstream of;
    of.open(filename, std::ios::binary | std::ios::trunc | std::ios::out);
    of.write(reinterpret_cast<const char*>(out), static_cast<std::streamsize>(outSize));
    of.close();

    // And free
    free(out);

    const auto sizeAfter = file_size(filename);
    callback.show_status(std::format(
        "{} after: {} bytes ({:.2}% smaller, {:.4}% compression)", 
        filename, 
        sizeAfter, 
        percentReduction(sizeBefore, sizeAfter), 
        percentReduction(static_cast<int>(data.size()), sizeAfter)));
}

void Utils::decompress(const std::string& filename)
{
    // Read file into memory
    std::vector<uint8_t> data;
    load_file(data, filename);

    // Write to disk, over the original file
    std::ofstream of;
    of.open(filename, std::ios::binary | std::ios::trunc | std::ios::out);
    of.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    of.close();
}

void Utils::load_file(std::vector<uint8_t>& buffer, const std::string& filename)
{
    const auto f = gzopen(filename.c_str(), "rb");
    if (f == nullptr)
    {
        throw std::runtime_error(std::format("Failed to open \"{}\"", filename));
    }
    // Read into the vector
    constexpr int chunkSize = 256 * 1024;
    while (gzeof(f) == 0)
    {
        // Make space
        const auto sizeBefore = buffer.size();
        buffer.resize(sizeBefore + chunkSize);
        // Read into it
        const auto amountRead = gzread(f, buffer.data() + sizeBefore, chunkSize);
        if (amountRead < 0)
        {
            int errorNumber;
            const char* error = gzerror(f, &errorNumber);
            throw std::runtime_error(std::format("Error reading/decompressing \"{}\": {}: {}", filename, errorNumber, error));
        }
        // Resize to fit
        buffer.resize(sizeBefore + amountRead);
    }
    gzclose(f);
}

// makes a unique temp filename out of src
// it will probably choke with weird parameters, real writable filenames should be OK
std::string Utils::make_temp_filename(const std::string& src)
{
    const auto directory = std::filesystem::canonical(src).parent_path();

    for (int i = 0; ; ++i)
    {
        auto testPath = directory / std::format("{}.tmp", i);
        if (!file_exists(testPath.string()))
        {
            return testPath.string();
        }
    }
}

std::string Utils::make_suffixed_filename(const std::string& src, const std::string& suffix)
{
    std::filesystem::path p(src);
    const auto& newFilename = std::format("{} ({}){}", p.stem().string(), suffix, p.extension().string());
    return p.replace_filename(newFilename).string();
}

// Delete destination, rename source source its name
void Utils::replace_file(const std::string& destination, const std::string& source)
{
    if (destination == source)
    {
        return;
    }
    if (file_exists(destination))
    {
        std::filesystem::remove(destination);
    }
    std::filesystem::rename(source, destination);
}

std::string Utils::to_lower(const std::string& s)
{
    std::string result;
    std::ranges::transform(
        s,
        result.begin(),
        [](const std::string::value_type c)
        {
            return static_cast<std::string::value_type>(std::tolower(c));
        });
    return result;
}

int Utils::make_word(const int b1, const int b2)
{
    return ((b1 & 0xff) << 0) |
        ((b2 & 0xff) << 8);
}

std::string Utils::samples_to_display_text(const uint32_t samples, const bool withMilliseconds)
{
    if (samples == 0)
    {
        return "-";
    }
    int seconds = static_cast<int>(samples) / 44100;
    int minutes = seconds / 60;
    seconds %= 60;
    if (withMilliseconds)
    {
        auto milliseconds = static_cast<double>(samples % 44100u) / 44100.0 + seconds;
        return std::format("{}:{:06.3f}", minutes, milliseconds);
    }
    return std::format("{}:{:02}", minutes, seconds);
}

std::string Utils::note_name(const double frequencyHz)
{
    if (frequencyHz < 1)
    {
        // ReSharper disable once StringLiteralTypo
        return "notanote";
    }

    const double midiNote = (log(frequencyHz) - log(440)) / log(2) * 12 + 69;
    const int nearestNote = static_cast<int>(std::round(midiNote));
    const char* noteNames[] = {"A", "A#", "B", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#"};
    return std::format(
        "{:>2}{:<2} {:>+3}",
        noteNames[abs(nearestNote - 21) % 12],
        (nearestNote - 24) / 12 + 1,
        static_cast<int>((midiNote - nearestNote) * 100 + ((nearestNote < midiNote)
            ? +0.5
            : -0.5))
    );
}

const std::string& Utils::on_off(const uint8_t value, const int bitIndex)
{
    static const std::string on("on");
    static const std::string off("off");
    return bit_set(value, bitIndex) ? on : off;
}

int Utils::bit_value(const uint8_t value, const int bitIndex)
{
    return (value >> bitIndex) & 1;
}

bool Utils::bit_set(const uint8_t value, const int bitIndex)
{
    return bit_value(value, bitIndex) == 1;
}

double Utils::db_to_percent(const double attenuation)
{
    return std::pow(10, -0.1 * attenuation) * 100;
}

double Utils::percentReduction(int before, int after)
{
    return static_cast<double>(before - after) / static_cast<double>(before) * 100;
}

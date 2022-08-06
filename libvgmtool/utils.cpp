#include <cstdio>
#include <cstdlib>
#include <zlib.h>
#include <filesystem>
#include "utils.h"

#include <fstream>
#include <stdexcept>
#include <vector>
#include <zopfli.h>

#include "IVGMToolCallback.h"

// Buffer for copying (created when needed)
#define BUFFER_SIZE 1024*8

bool Utils::file_exists(const std::string& filename)
{
    return std::filesystem::exists(filename);
}

// returns the size of the file in bytes
int Utils::file_size(const std::string& filename)
{
    return static_cast<int>(std::filesystem::file_size(filename));
}

void Utils::compress(const std::string& filename, int iterations)
{
    // Read file into memory
    std::vector<uint8_t> data;
    load_file(data, filename);

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

    // Write to disk, over the original file
    std::ofstream of;
    of.open(filename, std::ios::binary | std::ios::trunc | std::ios::out);
    of.write(reinterpret_cast<const char*>(out), static_cast<std::streamsize>(outSize));
    of.close();

    // And free
    free(out);
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
    auto f = gzopen(filename.c_str(), "rb");
    if (f == nullptr)
    {
        throw std::runtime_error(format("Failed to open \"%s\"", filename.c_str()));
    }
    // Read into the vector
    constexpr int chunkSize = 256 * 1024;
    while (!gzeof(f))
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
            throw std::runtime_error(format("Error reading/decompressing \"%s\": %d: %s", filename.c_str(), errorNumber, error));
        }
        // Resize to fit
        buffer.resize(sizeBefore + amountRead);
    }
    gzclose(f);
}

// makes a unique temp filename out of src
// it will probably choke with weird parameters, real writable filenames should be OK
std::string make_temp_filename(const std::string& src)
{
    auto directory = std::filesystem::canonical(src).parent_path();

    for (int i = 0; ; ++i)
    {
        auto testPath = directory / Utils::format("%d.tmp", i);
        if (!Utils::file_exists(testPath.string()))
        {
            return testPath.string();
        }
    }
}

//----------------------------------------------------------------------------------------------
// Helper routine - "filename.ext", "suffix" becomes "filename (suffix).ext"
//----------------------------------------------------------------------------------------------
std::string make_suffixed_filename(const std::string& src, const std::string& suffix)
{
    std::filesystem::path p(src);
    const auto& newFilename = Utils::format("%s (%s)%s", p.stem().string().c_str(), suffix.c_str(), p.extension().string().c_str());
    return p.replace_filename(newFilename).string();
}


// compresses the file with GZip compression
// to a temp file, then overwrites the original file with the temp
bool compress(const std::string& filename, const IVGMToolCallback& callback)
{
    int amtRead;

    if (!Utils::file_exists(filename))
    {
        return false;
    }

    callback.show_status("Compressing...");

    const auto outFilename = make_temp_filename(filename);

    gzFile out = gzopen(outFilename.c_str(), "wb9");
    gzFile in = gzopen(filename.c_str(), "rb");

    auto copybuffer = static_cast<char*>(malloc(BUFFER_SIZE));

    do
    {
        amtRead = gzread(in, copybuffer, BUFFER_SIZE);
        if (gzwrite(out, copybuffer, amtRead) != amtRead)
        {
            // Error copying file
            callback.show_error(Utils::format("Error copying data to temporary file %s!", outFilename.c_str()));
            free(copybuffer);
            gzclose(in);
            gzclose(out);
            std::filesystem::remove(outFilename);
            return false;
        }
    }
    while (amtRead > 0);

    free(copybuffer);
    gzclose(in);
    gzclose(out);

    replace_file(filename.c_str(), outFilename.c_str());

    callback.show_status("Compression complete");
    return true;
}

// decompress the file
// to a temp file, then overwrites the original file with the temp file
bool decompress(const std::string& filename, const IVGMToolCallback& callback)
{
    try
    {
        Utils::decompress(filename);
    }
    catch (const std::exception& e)
    {
        callback.show_error(Utils::format("Error decompressing \"%s\": %s", filename.c_str(), e.what()));
        return false;
    }
    callback.show_status("Decompression complete");
    return true;
}

// Assumes filename has space at the end for the extension, if needed
void change_ext(char* filename, const char* ext)
{
    char* p = strrchr(filename, '\\');
    char* q = strchr(p, '.');
    if (q == nullptr)
    {
        q = p + strlen(p); // if no ext, point to end of string
    }

    strcpy(q, ".");
    strcpy(q + 1, ext);
}

// Delete filetoreplace, rename with with its name
void replace_file(const char* filetoreplace, const char* with)
{
    if (strcmp(filetoreplace, with) == 0)
    {
        return;
    }
    if (Utils::file_exists(filetoreplace))
    {
        std::filesystem::remove(filetoreplace);
    }
    std::filesystem::rename(with, filetoreplace);
}

std::string Utils::format(const char* format, ...)
{
    // Copy args to test length
    va_list args;
    va_list args_copy;

    va_start(args, format);
    va_copy(args_copy, args);

    const int lengthNeeded = vsnprintf(nullptr, 0, format, args); // NOLINT(clang-diagnostic-format-nonliteral)
    if (lengthNeeded < 0)
    {
        va_end(args_copy);
        va_end(args);
        throw std::runtime_error("vsnprintf error");
    }

    std::string result;
    if (lengthNeeded > 0)
    {
        // Make a vector as it needs to be mutable
        std::vector<char> buffer(lengthNeeded + 1);
        vsnprintf(&buffer[0], buffer.size(), format, args_copy); // NOLINT(clang-diagnostic-format-nonliteral)
        // Then copy into a string
        result = std::string(&buffer[0], lengthNeeded);
    }

    va_end(args_copy);
    va_end(args);

    return result;
}

int Utils::make_word(const int b1, const int b2)
{
    return ((b1 & 0xff) << 0) |
        ((b2 & 0xff) << 8);
}

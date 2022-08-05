#include <cstdio>
#include <cstdlib>
#include <zlib.h>
#include <filesystem>
#include "utils.h"

#include <stdexcept>
#include <vector>

#include "IVGMToolCallback.h"

// Buffer for copying (created when needed)
#define BUFFER_SIZE 1024*8

// returns a boolean specifying if the passed filename exists
// also shows an error message if it doesn't
bool file_exists(const std::string& filename, const IVGMToolCallback& callback)
{
    const bool result = Utils::file_exists(filename);
    if (!result)
    {
        callback.show_error(Utils::format("File not found or in use:\n%s", filename.c_str()));
    }
    return result;
}

bool Utils::file_exists(const std::string& filename)
{
    return std::filesystem::exists(filename);
}

// returns the size of the file in bytes
int Utils::file_size(const std::string& filename)
{
    return static_cast<int>(std::filesystem::file_size(filename));
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
char* make_suffixed_filename(const char* src, const char* suffix, const IVGMToolCallback& callback)
{
    auto dest = static_cast<char*>(malloc(strlen(src) + strlen(suffix) + 10)); // 10 is more than I need to be safe

    strcpy(dest, src);
    char* p = strrchr(strrchr(dest, '\\'), '.'); // find last dot after last slash
    if (!p)
    {
        p = dest + strlen(dest); // if no extension, add to the end of the file instead
    }
    sprintf(p, " (%s)%s", suffix, src + (p - dest));

    callback.show_message(Utils::format("Made a temp filename:\n%s\nfrom:\n%s", dest, src)); // debugging

    return dest;
}


// compresses the file with GZip compression
// to a temp file, then overwrites the original file with the temp
bool compress(const std::string& filename, const IVGMToolCallback& callback)
{
    int amtRead;

    if (!file_exists(filename, callback))
    {
        return false;
    }

    callback.show_status("Compressing...");

    auto outFilename = make_temp_filename(filename);

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
bool decompress(char* filename, const IVGMToolCallback& callback)
{
    if (!file_exists(filename, callback))
    {
        return false;
    }

    callback.show_status("Decompressing...");

    const char* outFilename = make_temp_filename(filename).c_str();

    FILE* out = fopen(outFilename, "wb");
    gzFile in = gzopen(filename, "rb");

    const auto copyBuffer = malloc(BUFFER_SIZE);

    do
    {
        const auto amountRead = gzread(in, copyBuffer, BUFFER_SIZE);
        const auto amountWritten = fwrite(copyBuffer, 1, amountRead, out);
        if (static_cast<int>(amountWritten) != amountRead)
        {
            // Error copying file
            callback.show_error(Utils::format("Error copying data to temporary file %s!", outFilename));
            free(copyBuffer);
            gzclose(in);
            fclose(out);
            std::filesystem::remove(outFilename);
            return false;
        }
    }
    while (!gzeof(in));

    free(copyBuffer);
    gzclose(in);
    fclose(out);

    replace_file(filename, outFilename);

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

#define GZMagic1 0x1f
#define GZMagic2 0x8b
// Changes the file's extension to vgm or vgz depending on whether it's compressed
bool FixExt(char* filename, const IVGMToolCallback& callback)
{
    if (!file_exists(filename, callback))
    {
        return false;
    }

    FILE* f = fopen(filename, "rb");
    int IsCompressed = ((fgetc(f) == GZMagic1) && (fgetc(f) == GZMagic2));
    fclose(f);

    auto newfilename = static_cast<char*>(malloc(strlen(filename) + 10)); // plenty of space, can't hurt

    strcpy(newfilename, filename);

    if (IsCompressed)
    {
        change_ext(newfilename, "vgz");
    }
    else
    {
        change_ext(newfilename, "vgm");
    }

    if (strcmp(newfilename, filename) != 0)
    {
        replace_file(newfilename, filename);
        // replaces any existing file with the new name, with the existing file
        strcpy(filename, newfilename);
    }

    free(newfilename);
    return true;
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
    std::filesystem::rename(filetoreplace, with);
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

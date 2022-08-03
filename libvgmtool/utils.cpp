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
bool FileExists(const char* filename, const IVGMToolCallback& callback)
{
    bool result = FileExistsQuiet(filename);
    if (!result)
    {
        callback.show_error(Utils::format("File not found or in use:\n%s", filename));
    }
    return result;
}

bool FileExistsQuiet(const char* filename)
{
    if (!filename)
    {
        return false;
    }
    FILE* f = fopen(filename, "rb");
    bool result = (f != nullptr);
    if (f)
    {
        fclose(f);
    }
    return result;
}

// returns the size of the file in bytes
unsigned long int FileSize(const char* filename)
{
    FILE* f;
    fopen_s(&f, filename, "r");
    fseek(f, 0, SEEK_END);
    const auto size = ftell(f);
    fclose(f);
    return size;
}

// makes a unique temp filename out of src, mallocing the space for the result
// so don't forget to free it when you're done
// it will probably choke with weird parameters, real writable filenames should be OK
char* make_temp_filename(const char* src)
{
    int i = 0;

    auto dest = static_cast<char*>(malloc(strlen(src) + 4)); // just in case it has no extension, some extra space
    strcpy(dest, src);
    char* p = strrchr(dest, '\\');
    p++;
    do
    {
        sprintf(p, "%d.tmp", i++);
    }
    while (FileExistsQuiet(dest)); // keep trying integers until one doesn't exist

    //  ShowMessage("Made a temp filename:\n%s\nfrom:\n%s",dest,src); // debugging

    return dest;
}

//----------------------------------------------------------------------------------------------
// Helper routine - "filename.ext","suffix" becomes "filename (suffix).ext"
//----------------------------------------------------------------------------------------------
char* MakeSuffixedFilename(const char* src, const char* suffix, const IVGMToolCallback& callback)
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
bool compress(const char* filename, const IVGMToolCallback& callback)
{
    int AmtRead;

    if (!FileExists(filename, callback))
    {
        return false;
    }

    callback.show_status("Compressing...");

    // Check filesize since big files take ages to compress
    /*
    if (
        (FileSize(filename) > 1024 * 1024 * 1) &&
        (ShowQuestion(
            "This uncompressed VGM is over 1MB so it'll take a while to compress.\n"
            "Do you want to skip compressing it?\n"
            "(You can compress it later using the \"compress file\" button on the Misc tab.)"
        ) == IDYES)
    )
    {
        ShowStatus("Compression skipped");
        return FALSE;
    }*/

    char* outfilename = make_temp_filename(filename);

    gzFile out = gzopen(outfilename, "wb9");
    gzFile in = gzopen(filename, "rb");

    auto copybuffer = static_cast<char*>(malloc(BUFFER_SIZE));

    do
    {
        AmtRead = gzread(in, copybuffer,BUFFER_SIZE);
        if (gzwrite(out, copybuffer, AmtRead) != AmtRead)
        {
            // Error copying file
            callback.show_error(Utils::format("Error copying data to temporary file %s!", outfilename));
            free(copybuffer);
            gzclose(in);
            gzclose(out);
            std::filesystem::remove(outfilename);
            return false;
        }
    }
    while (AmtRead > 0);

    free(copybuffer);
    gzclose(in);
    gzclose(out);

    MyReplaceFile(filename, outfilename);

    free(outfilename);
    callback.show_status("Compression complete");
    return true;
}

// decompress the file
// to a temp file, then overwrites the original file with the temp file
bool Decompress(char* filename, const IVGMToolCallback& callback)
{
    if (!FileExists(filename, callback))
    {
        return false;
    }

    callback.show_status("Decompressing...");

    char* outFilename = make_temp_filename(filename);

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

    MyReplaceFile(filename, outFilename);

    free(outFilename);
    callback.show_status("Decompression complete");
    return true;
}

// Assumes filename has space at the end for the extension, if needed
void ChangeExt(char* filename, const char* ext)
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
    if (!FileExists(filename, callback))
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
        ChangeExt(newfilename, "vgz");
    }
    else
    {
        ChangeExt(newfilename, "vgm");
    }

    if (strcmp(newfilename, filename) != 0)
    {
        MyReplaceFile(newfilename, filename);
        // replaces any existing file with the new name, with the existing file
        strcpy(filename, newfilename);
    }

    free(newfilename);
    return true;
}

// Delete filetoreplace, rename with with its name
void MyReplaceFile(const char* filetoreplace, const char* with)
{
    if (strcmp(filetoreplace, with) == 0)
    {
        return;
    }
    if (FileExistsQuiet(filetoreplace))
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

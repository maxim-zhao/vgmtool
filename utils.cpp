#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include <zlib.h>
#include "vgm.h"
#include "gui.h"
#include "utils.h"

// Buffer for copying (created when needed)
#define BUFFER_SIZE 1024*8

// returns a boolean specifying if the passed filename exists
// also shows an error message if it doesn't
BOOL FileExists(const char* filename)
{
    BOOL result = FileExistsQuiet(filename);
    if (!result) ShowError("File not found or in use:\n%s", filename);
    return result;
}

BOOL FileExistsQuiet(const char* filename)
{
    if (!filename) return FALSE;
    FILE* f = fopen(filename, "rb");
    BOOL result = (f != nullptr);
    if (f) fclose(f);
    return result;
}

// returns the size of the file in bytes
unsigned long int FileSize(const char* filename)
{
    HANDLE f = CreateFile(filename,GENERIC_READ,FILE_SHARE_READ, nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL, nullptr);
    unsigned long int s = GetFileSize(f, nullptr);
    CloseHandle(f);
    return s;
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
char* MakeSuffixedFilename(const char* src, const char* suffix)
{
    auto dest = static_cast<char*>(malloc(strlen(src) + strlen(suffix) + 10)); // 10 is more than I need to be safe

    strcpy(dest, src);
    char* p = strrchr(strrchr(dest, '\\'), '.'); // find last dot after last slash
    if (!p) p = dest + strlen(dest); // if no extension, add to the end of the file instead
    sprintf(p, " (%s)%s", suffix, src + (p - dest));

    ShowMessage("Made a temp filename:\n%s\nfrom:\n%s", dest, src); // debugging

    return dest;
}


// compresses the file with GZip compression
// to a temp file, then overwrites the original file with the temp
BOOL compress(const char* filename)
{
    int AmtRead;

    if (!FileExists(filename)) return FALSE;

    ShowStatus("Compressing...");

    // Check filesize since big files take ages to compress
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
    }

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
            ShowError("Error copying data to temporary file %s!", outfilename);
            free(copybuffer);
            gzclose(in);
            gzclose(out);
            DeleteFile(outfilename);
            return FALSE;
        }
    }
    while (AmtRead > 0);

    free(copybuffer);
    gzclose(in);
    gzclose(out);

    MyReplaceFile(filename, outfilename);

    free(outfilename);
    ShowStatus("Compression complete");
    return TRUE;
}

// decompress the file
// to a temp file, then overwrites the original file with the temp file
BOOL Decompress(char* filename)
{
    int x;

    if (!FileExists(filename)) return FALSE;

    ShowStatus("Decompressing...");

    char* outfilename = make_temp_filename(filename);

    FILE* out = fopen(outfilename, "wb");
    gzFile in = gzopen(filename, "rb");

    auto copybuffer = static_cast<char*>(malloc(BUFFER_SIZE));

    do
    {
        int AmtRead = gzread(in, copybuffer,BUFFER_SIZE);
        if ((x = fwrite(copybuffer, 1, AmtRead, out)) != AmtRead)
        {
            // Error copying file
            ShowError("Error copying data to temporary file %s!", outfilename);
            free(copybuffer);
            gzclose(in);
            fclose(out);
            DeleteFile(outfilename);
            return FALSE;
        }
    }
    while (!gzeof(in));

    free(copybuffer);
    gzclose(in);
    fclose(out);

    MyReplaceFile(filename, outfilename);

    free(outfilename);
    ShowStatus("Decompression complete");
    return TRUE;
}

// Assumes filename has space at the end for the extension, if needed
void ChangeExt(char* filename, const char* ext)
{
    char* p = strrchr(filename, '\\');
    char* q = strchr(p, '.');
    if (q == nullptr) q = p + strlen(p); // if no ext, point to end of string

    strcpy(q, ".");
    strcpy(q + 1, ext);
}

#define GZMagic1 0x1f
#define GZMagic2 0x8b
// Changes the file's extension to vgm or vgz depending on whether it's compressed
BOOL FixExt(char* filename)
{
    if (!FileExists(filename)) return FALSE;

    FILE* f = fopen(filename, "rb");
    int IsCompressed = ((fgetc(f) == GZMagic1) && (fgetc(f) == GZMagic2));
    fclose(f);

    auto newfilename = static_cast<char*>(malloc(strlen(filename) + 10)); // plenty of space, can't hurt

    strcpy(newfilename, filename);

    if (IsCompressed) ChangeExt(newfilename, "vgz");
    else ChangeExt(newfilename, "vgm");

    if (strcmp(newfilename, filename) != 0)
    {
        MyReplaceFile(newfilename, filename); // replaces any existing file with the new name, with the existing file
        strcpy(filename, newfilename);
    }

    free(newfilename);
    return TRUE;
}

// Delete filetoreplace, rename with with its name
void MyReplaceFile(const char* filetoreplace, const char* with)
{
    if (strcmp(filetoreplace, with) == 0)
        return;
    if (FileExistsQuiet(filetoreplace))
        DeleteFile(filetoreplace);
    if (MoveFile(with, filetoreplace) == 0)
        ShowError("Error replacing old file:\n%s\nUpdated file is called:\n%s", filetoreplace, with);
}

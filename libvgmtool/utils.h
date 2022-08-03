#pragma once

// General purpose functions, not specific to anything much
// except most are VGM-centric

#include <string>

class IVGMToolCallback;

bool FileExists(const char* filename, const IVGMToolCallback& callback);
bool FileExistsQuiet(const char* filename);

unsigned long int FileSize(const char* filename);

bool compress(const char* filename, const IVGMToolCallback& callback);

bool Decompress(char* filename, const IVGMToolCallback& callback);

bool FixExt(char* FileName, const IVGMToolCallback& callback);

void ChangeExt(char* filename, const char* ext);

char* make_temp_filename(const char* src);
char* MakeSuffixedFilename(const char* src, const char* suffix, const IVGMToolCallback& callback);

void MyReplaceFile(const char* filetoreplace, const char* with);

#define ROUND(x) ((int)(x>0?x+0.5:x-0.5))

class Utils
{
public:
#if defined(__RESHARPER__) || defined(__GNUC__)
    [[gnu::format(printf, 1, 2)]]
#endif
    static std::string format(const char* format, ...);

    static int make_word(int b1, int b2);
};

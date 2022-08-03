#pragma once

// General purpose functions, not specific to anything much
// except most are VGM-centric

#include <Windows.h>
#include <string>

class IVGMToolCallback;

BOOL FileExists(const char* filename, const IVGMToolCallback& callback);
BOOL FileExistsQuiet(const char* filename);

unsigned long int FileSize(const char* filename);

BOOL compress(const char* filename, const IVGMToolCallback& callback);

BOOL Decompress(char* filename, const IVGMToolCallback& callback);

BOOL FixExt(char* FileName, const IVGMToolCallback& callback);

void ChangeExt(char* filename, const char* ext);

char* make_temp_filename(const char* src);
char* MakeSuffixedFilename(const char* src, const char* suffix, const IVGMToolCallback& callback);

void MyReplaceFile(const char* filetoreplace, const char* with, const IVGMToolCallback& callback);

#define ROUND(x) ((int)(x>0?x+0.5:x-0.5))

class Utils
{
public:
	static std::string format(const char* format, ...);
};
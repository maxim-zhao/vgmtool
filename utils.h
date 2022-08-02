#pragma once

// General purpose functions, not specific to anything much
// except most are VGM-centric

#include <Windows.h>

BOOL FileExists(const char* filename);
BOOL FileExistsQuiet(const char* filename);

unsigned long int FileSize(const char* filename);

BOOL compress(const char* filename);

BOOL Decompress(char* filename);

BOOL FixExt(char* FileName);

void ChangeExt(char* filename, const char* ext);

char* make_temp_filename(const char* src);
char* MakeSuffixedFilename(const char* src, const char* suffix);

void MyReplaceFile(const char* filetoreplace, const char* with);

#define ROUND(x) ((int)(x>0?x+0.5:x-0.5))

// General purpose functions, not specific to anything much
// except most are VGM-centric

#ifndef UTILS_H
#define UTILS_H

#include <Windows.h>

BOOL FileExists(char* filename);
BOOL FileExistsQuiet(char* filename);

unsigned long int FileSize(char* filename);

BOOL Compress(char* filename);

BOOL Decompress(char* filename);

BOOL FixExt(char* FileName);

void ChangeExt(char* filename, const char* ext);

char* MakeTempFilename(char* src);
char* MakeSuffixedFilename(const char* src, const char* suffix);

void MyReplaceFile(char* filetoreplace, char* with);

#define ROUND(x) ((int)(x>0?x+0.5:x-0.5))

#endif

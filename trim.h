#pragma once

#include <Windows.h>

void LogTrim(char* VGMFile, int start, int loop, int end);

BOOL NewTrim(char* filename, long int start, long int loop, long int end);

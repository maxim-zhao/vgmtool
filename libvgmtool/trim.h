#pragma once

#include <Windows.h>

class IVGMToolCallback;

void log_trim(char* VGMFile, int start, int loop, int end, const IVGMToolCallback& callback);

BOOL new_trim(char* filename, int start, int loop, int end, const IVGMToolCallback& callback);

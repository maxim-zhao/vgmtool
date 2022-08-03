#pragma once

class IVGMToolCallback;

void log_trim(char* VGMFile, int start, int loop, int end, const IVGMToolCallback& callback);

bool new_trim(char* filename, int start, int loop, int end, const IVGMToolCallback& callback);

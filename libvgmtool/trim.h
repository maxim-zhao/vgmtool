#pragma once
#include <xstring>

class IVGMToolCallback;

void log_trim(const std::string& VGMFile, int start, int loop, int end, const IVGMToolCallback& callback);

bool new_trim(const std::string& filename, int start, int loop, int end, const IVGMToolCallback& callback);

void trim(const std::string& filename, int start, int loop, int end, bool overWrite, bool logTrims, const IVGMToolCallback& callback);

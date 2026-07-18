#pragma once
#include <string>

class IVGMToolCallback;
class VgmFile;

void log_trim(const std::string& filename, int start, int loop, int end, const IVGMToolCallback& callback);

void trim(const std::string& filename, int start, int loop, int end, bool overWrite, bool logTrims, const IVGMToolCallback& callback, std::string outFilename);

// Re-implemented trim that works on VgmFile in-memory instead of streaming file data
void trim_vgm_file(VgmFile& vgmFile, int start, int loop, int end, const IVGMToolCallback& callback);


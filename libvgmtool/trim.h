#pragma once
#include <string>

class IStatusCallback;
class VgmFile;

void log_trim(const std::string& filename, int start, int loop, int end, const IStatusCallback& callback);

void trim(const std::string& filename, int start, int loop, int end, bool overWrite, bool logTrims, const IStatusCallback& callback, std::string outFilename);

// Re-implemented trim that works on VgmFile in-memory instead of streaming file data
void trim_vgm_file(VgmFile& vgmFile, int start, int loop, int end, const IStatusCallback& callback);


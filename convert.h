#pragma once
#include <string>
// Conversion routines

enum class convert_file_type { gym, ssl, cym };

bool convert_to_vgm(const std::string& filename, convert_file_type FileType);


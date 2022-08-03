#pragma once
#include <string>

class IVGMToolCallback;

void write_to_text(const std::string& filename, const IVGMToolCallback& callback);

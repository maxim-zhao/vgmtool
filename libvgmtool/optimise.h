#pragma once
#include <string>

// VGM optimisation

class IVGMToolCallback;

bool optimise_vgm_pauses(const std::string& filename, const IVGMToolCallback& callback);

int remove_offset(const std::string& filename, const IVGMToolCallback& callback);

bool round_to_frame_accurate(const std::string& filename, const IVGMToolCallback& callback);

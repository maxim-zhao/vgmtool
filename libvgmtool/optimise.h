#pragma once
#include <string>

// VGM optimisation

class IStatusCallback;

bool optimise_vgm_pauses(const std::string& filename, const IStatusCallback& callback);

int remove_offset(const std::string& filename, const IStatusCallback& callback);

bool round_to_frame_accurate(const std::string& filename, const IStatusCallback& callback);

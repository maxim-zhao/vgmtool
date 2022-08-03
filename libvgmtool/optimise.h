#pragma once

// VGM optimisation

class IVGMToolCallback;

bool optimise_vgm_pauses(char* filename, const IVGMToolCallback& callback);

int remove_offset(char* filename, const IVGMToolCallback& callback);

bool optimise_vgm_data(char* filename);

bool round_to_frame_accurate(char* filename, const IVGMToolCallback& callback);

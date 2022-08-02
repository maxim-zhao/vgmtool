#pragma once

// VGM optimisation

#include <Windows.h>

BOOL OptimiseVGMPauses(char* filename);

int RemoveOffset(char* filename);

BOOL OptimiseVGMData(char* filename);

BOOL RoundToFrameAccurate(char* filename);

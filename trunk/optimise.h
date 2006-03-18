// VGM optimisation

#ifndef OPTIMISE_H
#define OPTIMISE_H

#include <windows.h>

BOOL OptimiseVGMPauses(char *filename);

int RemoveOffset(char *filename);

BOOL OptimiseVGMData(char *filename);

BOOL RoundToFrameAccurate(char *filename);

#endif
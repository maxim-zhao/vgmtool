#ifndef TRIM_H
#define TRIM_H

#include <windows.h>

void LogTrim(char *VGMFile,int start,int loop,int end);

BOOL NewTrim(char *filename,const long int start, const long int loop, const long int end);

#endif
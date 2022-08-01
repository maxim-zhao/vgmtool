// Conversion routines

#ifndef CONVERT_H
#define CONVERT_H

#include <Windows.h>

enum ConvertFileType { ftGYM, ftSSL, ftCYM };

BOOL ConverttoVGM(char* filename, int FileType);

#endif

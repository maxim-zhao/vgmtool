// Conversion routines

#ifndef CONVERT_H
#define CONVERT_H

#include <windows.h>

enum ConvertFileType { ftGYM, ftSSL, ftCYM };

BOOL ConverttoVGM(char *filename,int FileType);

#endif
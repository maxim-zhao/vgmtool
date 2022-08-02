#pragma once

// Access to GUI stuff for various functions
// Very OS-dependent

#include <Windows.h>

#if defined(__RESHARPER__) || defined(__GNUC__)
    #define PRINTF_ATTR(StringIndex, FirstToCheck) \
        [[gnu::format(printf, StringIndex, FirstToCheck)]]
#else
    #define PRINTF_ATTR(StringIndex, FirstToCheck)
#endif


PRINTF_ATTR(1, 2)
void ShowMessage(const char* format, ...);

PRINTF_ATTR(1, 2)
void ShowError(const char* format, ...);

PRINTF_ATTR(1, 2)
int ShowQuestion(const char* format, ...);

PRINTF_ATTR(1, 2)
void ShowStatus(const char* format, ...);

PRINTF_ATTR(1, 2)
void add_convert_text(const char* format, ...);

bool get_int(HWND hDlg, int item, int* result);

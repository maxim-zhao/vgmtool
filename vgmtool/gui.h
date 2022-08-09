#pragma once

// Access to GUI stuff for various functions
// Very OS-dependent

#include <string>
#include <Windows.h>

#if defined(__RESHARPER__) || defined(__GNUC__)
    #define PRINTF_ATTR(StringIndex, FirstToCheck) \
        [[gnu::format(printf, StringIndex, FirstToCheck)]]
#else
    #define PRINTF_ATTR(StringIndex, FirstToCheck)
#endif

PRINTF_ATTR(1, 2)
void show_message_box(const char* format, ...);

PRINTF_ATTR(1, 2)
void show_error_message_box(const char* format, ...);

PRINTF_ATTR(1, 2)
int show_question_message_box(const char* format, ...);

PRINTF_ATTR(1, 2)
void set_status_text(const char* format, ...);

PRINTF_ATTR(1, 2)
void add_convert_text(const char* format, ...);

bool get_int(HWND hDlg, int item, int* result);

std::string get_string(HWND hDlg, int item);

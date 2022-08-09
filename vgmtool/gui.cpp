#include <Windows.h>
#include <cstdio>
#include <cstdarg>
#include "gui.h"

#include <stdexcept>
#include <string>

#include "resource.h"

extern HWND hWndMain;
extern std::string g_program_name;

#define NumTabChildWnds 6
extern HWND TabChildWnds[NumTabChildWnds];
#define ConvertWnd  TabChildWnds[4]

void show_error_message_box(const char* format, ...)
{
    va_list args;
    char buffer[1024];
    va_start(args, format); // varargs start after format
    _vsnprintf(buffer, 1024, format, args);
    va_end(args); // clean things up before leaving
    MessageBox(hWndMain, buffer, g_program_name.c_str(), MB_ICONERROR + MB_OK);
}

void show_message_box(const char* format, ...)
{
    va_list args;
    char buffer[1024];
    va_start(args, format); // varargs start after format
    _vsnprintf(buffer, 1024, format, args);
    va_end(args); // clean things up before leaving
    MessageBox(hWndMain, buffer, g_program_name.c_str(), 0);
}

int show_question_message_box(const char* format, ...)
{
    va_list args;
    char buffer[1024];
    va_start(args, format); // varargs start after format
    _vsnprintf(buffer, 1024, format, args);
    va_end(args); // clean things up before leaving
    return MessageBox(hWndMain, buffer, g_program_name.c_str(), MB_ICONQUESTION + MB_YESNO);
}


void set_status_text(const char* format, ...)
{
    va_list args;
    char buffer[1024];
    va_start(args, format); // varargs start after format
    _vsnprintf(buffer, 1024, format, args);
    va_end(args); // clean things up before leaving
    SetDlgItemText(hWndMain, txtStatusBar, buffer);
}

void add_convert_text(const char* format, ...)
{
    va_list args;
    char buffer[1024];
    va_start(args, format); // varargs start after format
    _vsnprintf(buffer, 1024, format, args);
    va_end(args); // clean things up before leaving

    const auto length = SendDlgItemMessage(ConvertWnd, edtConvertResults, WM_GETTEXTLENGTH, 0, 0); // Get length
    SendDlgItemMessage(ConvertWnd, edtConvertResults, EM_SETSEL, length, length); // move caret to end of text
    SendDlgItemMessage(ConvertWnd, edtConvertResults, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(buffer));
    // insert text there
}

bool get_int(HWND hDlg, int item, int* result)
{
    BOOL success;
    const auto value = GetDlgItemInt(hDlg, item, &success, FALSE);
    if (success == TRUE || value != 0)
    {
        // success == TRUE -> no errors
        // success == FALSE but value != 0 -> there were errors, but it managed to convert an integer
        // This is the case when the text is something like "1234 (text)"
        *result = static_cast<int>(value);
        return true;
    }
    // However, text like "0 (text)" looks like a total failure.
    // But I can test for that.
    char c[3];
    if (GetDlgItemText(hDlg, item, c, 3) == 2 && c[0] == '0' && !isdigit(c[1]))
    {
        // It looks like it was like that
        *result = 0;
        return true;
    }

    return false;
}

std::string get_string(HWND hDlg, int item)
{
    const auto length = GetWindowTextLength(GetDlgItem(hDlg, item)) + 1;
    std::string s(length, L'\0');
    if (length > 0)
    {
        // Non-empty string
        if (static_cast<int>(GetDlgItemText(hDlg, item, s.data(), static_cast<int>(s.size()))) == 0)
        {
            throw std::runtime_error("Failed to get text");
        }
    }
    return s;
}

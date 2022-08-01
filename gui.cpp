#include <Windows.h>
#include <cstdio>
#include <cstdarg>
#include "gui.h"
#include "resource.h"

extern HWND hWndMain;
extern const char* ProgName;

#define NumTabChildWnds 6
extern HWND TabChildWnds[NumTabChildWnds];
#define HeaderWnd   TabChildWnds[0]
#define TrimWnd     TabChildWnds[1]
#define StripWnd    TabChildWnds[2]
#define GD3Wnd      TabChildWnds[3]
#define ConvertWnd  TabChildWnds[4]
#define MiscWnd     TabChildWnds[5]

void ShowError(const char* format, ...)
{
    va_list args;
    char buffer[1024];
    va_start(args, format); // varargs start after format
    _vsnprintf(buffer, 1024, format, args);
    va_end(args); // clean things up before leaving
    MessageBox(hWndMain, buffer, ProgName,MB_ICONERROR + MB_OK);
}

void ShowMessage(const char* format, ...)
{
    va_list args;
    char buffer[1024];
    va_start(args, format); // varargs start after format
    _vsnprintf(buffer, 1024, format, args);
    va_end(args); // clean things up before leaving
    MessageBox(hWndMain, buffer, ProgName, 0);
}

int ShowQuestion(const char* format, ...)
{
    va_list args;
    char buffer[1024];
    va_start(args, format); // varargs start after format
    _vsnprintf(buffer, 1024, format, args);
    va_end(args); // clean things up before leaving
    return MessageBox(hWndMain, buffer, ProgName,MB_ICONQUESTION + MB_YESNO);
}


void ShowStatus(const char* format, ...)
{
    va_list args;
    char buffer[1024];
    va_start(args, format); // varargs start after format
    _vsnprintf(buffer, 1024, format, args);
    va_end(args); // clean things up before leaving
    SetDlgItemText(hWndMain,txtStatusBar, buffer);
}

// wnd, id specify a multi-line edit box
// format and subsequent printf-style parameters define a string to append to it
void AddConvertText(const char* format, ...)
{
    va_list args;
    char buffer[1024];
    va_start(args, format); // varargs start after format
    _vsnprintf(buffer, 1024, format, args);
    va_end(args); // clean things up before leaving

    int length = SendDlgItemMessage(ConvertWnd,edtConvertResults,WM_GETTEXTLENGTH, 0, 0); // Get length
    SendDlgItemMessage(ConvertWnd,edtConvertResults,EM_SETSEL, length, length); // move caret to end of text
    SendDlgItemMessage(ConvertWnd,edtConvertResults,EM_REPLACESEL,FALSE, (LPARAM)buffer); // insert text there
}

BOOL GetInt(HWND hDlg, int item, int* result)
{
    BOOL b;
    char c[3];
    int i = GetDlgItemInt(hDlg, item, &b,FALSE);
    if (b || i)
    {
        // b==TRUE -> no errors
        // b==FALSE but i!=0 -> there were errors, but it managed to convert an integer
        // This is the case when the text is something like "1234 (text)"
        *result = i;
        return TRUE;
    }
    // However, text like "0 (text)" looks like a total failure.
    // But I can test for that.
    if (
        (GetDlgItemText(hDlg, item, c, 3) == 2) &&
        (c[0] == '0') &&
        !isdigit(c[1])
    )
    {
        // It looks like it was like that
        *result = 0;
        return TRUE;
    }

    return FALSE;
}

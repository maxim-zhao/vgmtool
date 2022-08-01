// Access to GUI stuff for various functions
// Very OS-dependent

#ifndef GUI_H
#define GUI_H

#include <Windows.h>

void ShowMessage(const char* format, ...);
void ShowError(const char* format, ...);
int ShowQuestion(const char* format, ...);

void ShowStatus(const char* format, ...);

void AddConvertText(const char* format, ...);

BOOL GetInt(HWND hDlg, int item, int* result);

#endif

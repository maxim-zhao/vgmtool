// Access to GUI stuff for various functions
// Very OS-dependent

#ifndef GUI_H
#define GUI_H

#include <windows.h>

void ShowMessage(char *format,...);
void ShowError(char *format,...);
int  ShowQuestion(char *format,...);

void ShowStatus(char *format,...);

void AddConvertText(char *format,...);

BOOL GetInt(HWND hDlg,int item,int *result);

#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include "gui.h"

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, const LPSTR lpCmdLine, const int nShowCmd)
{
    SetProcessDPIAware();

    Gui gui(hInstance, lpCmdLine, nShowCmd);
    gui.run();

    return 0;
}

#include "Application.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, PSTR, int) {

#if !NDEBUG
    if(!AttachConsole(ATTACH_PARENT_PROCESS)) {
        AllocConsole();
    }

    FILE *dummyFile;
    freopen_s(&dummyFile, "CONIN$", "r", stdin);
    freopen_s(&dummyFile, "CONOUT$", "w", stderr);
    freopen_s(&dummyFile, "CONOUT$", "w", stdout);
#endif

    Application app;
    app.run();

    return 0;
}

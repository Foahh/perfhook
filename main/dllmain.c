#include <stddef.h>
#include <stdlib.h>
#include <windows.h>

#include "hook/process.h"
#include "lib/d3d9/d3d9.h"
#include "lib/nvidia/nvidia.h"
#include "util/winapi.h"

static DWORD WINAPI entry(LPVOID lpParam) {
    nvapi_load();
    nvapi_set_profile_settings();
    set_current_priority_class(HIGH_PRIORITY_CLASS);
    d3d9_hook_load((HMODULE)lpParam);
    return 0;
}

BOOL WINAPI DllMain(HMODULE mod, DWORD cause, void *ctx) {
    if (cause == DLL_PROCESS_ATTACH) {
        CreateThread(NULL, 0, entry, (LPVOID)mod, 0, NULL);
    } else if (cause == DLL_PROCESS_DETACH) {
        d3d9_hook_unload();
        nvapi_unload();
    }
    return TRUE;
}

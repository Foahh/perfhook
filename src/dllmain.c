#include <stddef.h>
#include <stdlib.h>
#include <windows.h>

#include "d3d9_hook/d3d9_hook.h"
#include "hook/process.h"
#include "nvidia/nvidia.h"
#include "util/winapi.h"

static DWORD WINAPI entry(LPVOID lpParam) {
  nvapi_load();
  nvapi_set_profile_settings();
  set_current_priority_class(HIGH_PRIORITY_CLASS);
  d3d9_hook_load((HMODULE)lpParam);
  return 0;
}

BOOL WINAPI DllMain(HMODULE mod, DWORD cause, void *ctx) {
  HANDLE thread;

  (void)ctx;

  if (cause == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(mod);

    thread = CreateThread(NULL, 0, entry, (LPVOID)mod, 0, NULL);
    if (thread != NULL) {
      CloseHandle(thread);
    }
  }
  return TRUE;
}

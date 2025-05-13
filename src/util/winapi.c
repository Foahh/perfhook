#include "winapi.h"

#include <stddef.h>
#include <stdlib.h>
#include <windows.h>

#include "util/dprintf.h"

#define UTIL_LOG(...) dprintf("util: " __VA_ARGS__)

void set_current_priority_class(DWORD priority_class) {
  HANDLE h_process = GetCurrentProcess();
  if (h_process == NULL) return;
  BOOL result = SetPriorityClass(h_process, priority_class);
  if (!result) {
    UTIL_LOG("SetPriorityClass failed: %lu\n", (int)GetLastError());
    return;
  }

  char process_name[MAX_PATH];
  if (GetModuleFileNameA(NULL, process_name, sizeof(process_name)) == 0) {
    UTIL_LOG("GetModuleFileNameA failed: %lu\n", (int)GetLastError());
    return;
  }

  DWORD current_priority_class = GetPriorityClass(h_process);
  UTIL_LOG("%s -> priority class set to %lu\n", process_name,
           (int)current_priority_class);
}

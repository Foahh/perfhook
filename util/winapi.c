#include "winapi.h"

#include <stddef.h>
#include <stdlib.h>
#include <windows.h>

#include "dprintf.h"

void set_current_priority_class(DWORD priority_class) {
    HANDLE h_process = GetCurrentProcess();
    if (h_process == NULL) return;
    BOOL result = SetPriorityClass(h_process, priority_class);
    if (!result) {
        dprintf("util: SetPriorityClass failed: %lu\n", (int)GetLastError());
        return;
    }

    char process_name[MAX_PATH];
    if (GetModuleFileNameA(NULL, process_name, sizeof(process_name)) == 0) {
        dprintf("util: GetModuleFileNameA failed: %lu\n", (int)GetLastError());
        return;
    }

    DWORD current_priority_class = GetPriorityClass(h_process);
    dprintf("util: %s -> priority class set to %lu\n", process_name,
            (int)current_priority_class);
}

#include <assert.h>
#include <d3d9.h>
#include <stdlib.h>
#include <windows.h>

#include "hook/com-proxy.h"
#include "hook/table.h"
#include "hooklib/dll.h"
#include "util/dprintf.h"

typedef IDirect3D9*(WINAPI* Direct3DCreate9_t)(UINT sdk_ver);
typedef HRESULT(WINAPI* Direct3DCreate9Ex_t)(UINT sdk_ver,
                                             IDirect3D9Ex** d3d9ex);

static HRESULT STDMETHODCALLTYPE my_IDirect3D9_CreateDevice(
    IDirect3D9* self, UINT adapter, D3DDEVTYPE type, HWND hwnd, DWORD flags,
    D3DPRESENT_PARAMETERS* pp, IDirect3DDevice9** pdev);

static Direct3DCreate9_t next_Direct3DCreate9;
static Direct3DCreate9Ex_t next_Direct3DCreate9Ex;

static const struct hook_symbol gfx_hooks[] = {{
    .name = "Direct3DCreate9",
    .patch = Direct3DCreate9,
    .link = (void**)&next_Direct3DCreate9,
}};

static HMODULE d3d9;

void d3d9_hook_load(HINSTANCE self) {
    hook_table_apply(NULL, "d3d9.dll", gfx_hooks, _countof(gfx_hooks));

    if (next_Direct3DCreate9 == NULL || next_Direct3DCreate9Ex == NULL) {
        d3d9 = LoadLibraryW(L"d3d9.dll");

        if (d3d9 == NULL) {
            dprintf("d3d9Hook: d3d9.dll not found or failed initialization\n");
            goto fail;
        }

        if (next_Direct3DCreate9 == NULL) {
            next_Direct3DCreate9 =
                (Direct3DCreate9_t)GetProcAddress(d3d9, "Direct3DCreate9");
        }
        if (next_Direct3DCreate9Ex == NULL) {
            next_Direct3DCreate9Ex =
                (Direct3DCreate9Ex_t)GetProcAddress(d3d9, "Direct3DCreate9Ex");
        }

        if (next_Direct3DCreate9 == NULL) {
            dprintf("d3d9Hook: Direct3DCreate9 not found in loaded d3d9.dll\n");
            goto fail;
        }
        if (next_Direct3DCreate9Ex == NULL) {
            dprintf(
                "d3d9Hook: Direct3DCreate9Ex not found in loaded d3d9.dll\n");
            goto fail;
        }
    }

    if (self != NULL) {
        dll_hook_push(self, L"d3d9.dll");
    }

    return;

fail:
    if (d3d9 != NULL) {
        FreeLibrary(d3d9);
    }
}

IDirect3D9* WINAPI Direct3DCreate9(UINT sdk_ver) {
    struct com_proxy* proxy;
    IDirect3D9Vtbl* vtbl;
    IDirect3D9* api = NULL;
    HRESULT hr;

    dprintf("d3d9Hook: Direct3DCreate9 hook hit\n");

    api = NULL;

    if (next_Direct3DCreate9Ex != NULL) {
        IDirect3D9Ex* d3d9ex = NULL;
        hr = next_Direct3DCreate9Ex(sdk_ver, &d3d9ex);

        if (SUCCEEDED(hr) && d3d9ex != NULL) {
            api = (IDirect3D9*)d3d9ex;
            dprintf("d3d9Hook: successfully created D3D9Ex interface\n");
        }
    }

    if (api == NULL) {
        api = next_Direct3DCreate9(sdk_ver);
        if (api == NULL) {
            dprintf("d3d9Hook: next_Direct3DCreate9 returned NULL\n");
            goto fail;
        }
        dprintf("d3d9Hook: using standard D3D9 interface\n");
    }

    hr = com_proxy_wrap(&proxy, api, sizeof(*api->lpVtbl));
    if (FAILED(hr)) {
        dprintf("d3d9Hook: com_proxy_wrap returned %x\n", (int)hr);
        goto fail;
    }

    vtbl = proxy->vptr;
    vtbl->CreateDevice = my_IDirect3D9_CreateDevice;

    return (IDirect3D9*)proxy;

fail:
    if (api != NULL) {
        IDirect3D9_Release(api);
    }

    return NULL;
}

static HRESULT STDMETHODCALLTYPE my_IDirect3D9_CreateDevice(
    IDirect3D9* self, UINT adapter, D3DDEVTYPE type, HWND hwnd, DWORD flags,
    D3DPRESENT_PARAMETERS* pp, IDirect3DDevice9** pdev) {
    struct com_proxy* proxy;
    IDirect3D9* real;
    HRESULT hr;

    dprintf("d3d9Hook: IDirect3D9::CreateDevice hook hit\n");

    proxy = com_proxy_downcast(self);
    real = proxy->real;

    if (next_Direct3DCreate9Ex != NULL) {
        IDirect3D9Ex* d3d9ex = NULL;
        IDirect3DDevice9Ex* deviceEx = NULL;

        HRESULT result =
            IDirect3D9_QueryInterface(real, &IID_IDirect3D9Ex, (void**)&d3d9ex);
        if (SUCCEEDED(result)) {
            hr = IDirect3D9Ex_CreateDeviceEx(d3d9ex, adapter, type, hwnd, flags,
                                             pp, NULL, &deviceEx);
            IDirect3D9Ex_Release(d3d9ex);

            if (SUCCEEDED(hr)) {
                dprintf("d3d9Hook: successfully created D3D9Ex device\n");

                struct com_proxy* device_proxy;
                hr = com_proxy_wrap(&device_proxy, (IDirect3DDevice9*)deviceEx,
                                    sizeof(IDirect3DDevice9Vtbl));
                if (FAILED(hr)) {
                    IDirect3DDevice9Ex_Release(deviceEx);
                    return hr;
                }

                *pdev = (IDirect3DDevice9*)device_proxy;
                return hr;
            }
        }
    }

    dprintf("d3d9Hook: falling back to standard D3D9 device\n");
    return IDirect3D9_CreateDevice(real, adapter, type, hwnd, flags, pp, pdev);
}

void d3d9_hook_unload(void) {
    if (d3d9 != NULL) {
        dprintf("d3d9Hook: unloading d3d9.dll\n");
        FreeLibrary(d3d9);
        d3d9 = NULL;
    }
}

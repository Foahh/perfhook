#include <assert.h>
#include <d3d9.h>
#include <stdbool.h>
#include <stdlib.h>
#include <windows.h>

#include "hook/com-proxy.h"
#include "hook/procaddr.h"
#include "hook/table.h"
#include "hooklib/dll.h"
#include "util/dprintf.h"

#define D3D9_LOG(...) dprintf("d3d9hook: " __VA_ARGS__)

typedef IDirect3D9*(WINAPI* Direct3DCreate9_t)(UINT sdk_ver);
typedef HRESULT(WINAPI* Direct3DCreate9Ex_t)(UINT sdk_ver,
                                             IDirect3D9Ex** d3d9ex);

static HRESULT STDMETHODCALLTYPE my_IDirect3D9_CreateDevice(
    IDirect3D9* self, UINT adapter, D3DDEVTYPE type, HWND hwnd, DWORD flags,
    D3DPRESENT_PARAMETERS* pp, IDirect3DDevice9** pdev);
static HRESULT STDMETHODCALLTYPE my_IDirect3D9Ex_CreateDevice(
    IDirect3D9Ex* self, UINT adapter, D3DDEVTYPE type, HWND hwnd, DWORD flags,
    D3DPRESENT_PARAMETERS* pp, IDirect3DDevice9** pdev);
static HRESULT STDMETHODCALLTYPE my_IDirect3D9Ex_CreateDeviceEx(
    IDirect3D9Ex* self, UINT adapter, D3DDEVTYPE type, HWND hwnd, DWORD flags,
    D3DPRESENT_PARAMETERS* pp, D3DDISPLAYMODEEX* fullscreen_display_mode,
    IDirect3DDevice9Ex** pdev);
static HMODULE d3d9_hook_load_real(void);

static Direct3DCreate9_t next_Direct3DCreate9;
static Direct3DCreate9Ex_t next_Direct3DCreate9Ex;
static bool d3d9_hook_initted;

static const struct hook_symbol gfx_hooks[] = {
    {
        .name = "Direct3DCreate9",
        .patch = Direct3DCreate9,
        .link = (void**)&next_Direct3DCreate9,
    },
    {
        .name = "Direct3DCreate9Ex",
        .patch = Direct3DCreate9Ex,
        .link = (void**)&next_Direct3DCreate9Ex,
    },
};

static HMODULE d3d9;

void d3d9_hook_load(HINSTANCE self) {
  if (next_Direct3DCreate9 == NULL || next_Direct3DCreate9Ex == NULL) {
    d3d9 = d3d9_hook_load_real();

    if (d3d9 == NULL && next_Direct3DCreate9 == NULL) {
      D3D9_LOG("d3d9.dll not found or failed initialization\n");
      goto fail;
    }

    if (d3d9 != NULL && next_Direct3DCreate9 == NULL) {
      Direct3DCreate9_t proc =
          (Direct3DCreate9_t)GetProcAddress(d3d9, "Direct3DCreate9");
      if (next_Direct3DCreate9 == NULL) {
        next_Direct3DCreate9 = proc;
      }
    }
    if (d3d9 != NULL && next_Direct3DCreate9Ex == NULL) {
      Direct3DCreate9Ex_t proc =
          (Direct3DCreate9Ex_t)GetProcAddress(d3d9, "Direct3DCreate9Ex");
      if (next_Direct3DCreate9Ex == NULL) {
        next_Direct3DCreate9Ex = proc;
      }
    }

    if (next_Direct3DCreate9 == NULL) {
      D3D9_LOG("Direct3DCreate9 not found in loaded d3d9.dll\n");
      goto fail;
    }
  }

  if (next_Direct3DCreate9Ex == NULL) {
    D3D9_LOG("Direct3DCreate9Ex not available\n");
  }

  if (!d3d9_hook_initted) {
    hook_table_apply(NULL, "d3d9.dll", gfx_hooks, _countof(gfx_hooks));
    proc_addr_table_push(NULL, "d3d9.dll", gfx_hooks, _countof(gfx_hooks));
    d3d9_hook_initted = true;
  }

  if (self != NULL) {
    dll_hook_push(self, L"d3d9.dll");
  }

  return;

fail:
  if (d3d9 != NULL) {
    FreeLibrary(d3d9);
    d3d9 = NULL;
  }
}

static HMODULE d3d9_hook_load_real(void) {
  wchar_t path[MAX_PATH];
  UINT len;

  if (d3d9 != NULL) {
    return d3d9;
  }

  len = GetSystemDirectoryW(path, _countof(path));
  if (len == 0 || len >= _countof(path)) {
    return NULL;
  }

  if (wcscat_s(path, _countof(path), L"\\d3d9.dll") != 0) {
    return NULL;
  }

  return LoadLibraryW(path);
}

IDirect3D9* WINAPI Direct3DCreate9(UINT sdk_ver) {
  struct com_proxy* proxy;
  IDirect3D9Vtbl* vtbl;
  IDirect3D9* api = NULL;
  HRESULT hr;

  D3D9_LOG("Direct3DCreate9 hook hit\n");

  if (next_Direct3DCreate9 == NULL) {
    d3d9_hook_load(NULL);
  }
  if (next_Direct3DCreate9 == NULL) {
    D3D9_LOG("Direct3DCreate9 unavailable\n");
    return NULL;
  }

  api = NULL;
  api = next_Direct3DCreate9(sdk_ver);
  if (api == NULL) {
    D3D9_LOG("next_Direct3DCreate9 returned NULL\n");
    goto fail;
  }

  hr = com_proxy_wrap(&proxy, api, sizeof(*api->lpVtbl));
  if (FAILED(hr)) {
    D3D9_LOG("com_proxy_wrap returned %x\n", (int)hr);
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

HRESULT WINAPI Direct3DCreate9Ex(UINT sdk_ver, IDirect3D9Ex** d3d9ex) {
  struct com_proxy* proxy;
  IDirect3D9ExVtbl* vtbl;
  IDirect3D9Ex* api;
  HRESULT hr;

  D3D9_LOG("Direct3DCreate9Ex hook hit\n");

  if (d3d9ex == NULL) {
    return D3DERR_INVALIDCALL;
  }

  *d3d9ex = NULL;

  if (next_Direct3DCreate9Ex == NULL) {
    d3d9_hook_load(NULL);
  }
  if (next_Direct3DCreate9Ex == NULL) {
    return D3DERR_NOTAVAILABLE;
  }

  api = NULL;
  hr = next_Direct3DCreate9Ex(sdk_ver, &api);
  if (FAILED(hr)) {
    return hr;
  }
  if (api == NULL) {
    return D3DERR_NOTAVAILABLE;
  }

  hr = com_proxy_wrap(&proxy, api, sizeof(IDirect3D9ExVtbl));
  if (FAILED(hr)) {
    D3D9_LOG("com_proxy_wrap returned %x\n", (int)hr);
    IDirect3D9Ex_Release(api);
    return hr;
  }

  vtbl = proxy->vptr;
  vtbl->CreateDevice = my_IDirect3D9Ex_CreateDevice;
  vtbl->CreateDeviceEx = my_IDirect3D9Ex_CreateDeviceEx;

  *d3d9ex = (IDirect3D9Ex*)proxy;

  return D3D_OK;
}

static HRESULT STDMETHODCALLTYPE my_IDirect3D9_CreateDevice(
    IDirect3D9* self, UINT adapter, D3DDEVTYPE type, HWND hwnd, DWORD flags,
    D3DPRESENT_PARAMETERS* pp, IDirect3DDevice9** pdev) {
  struct com_proxy* proxy;
  IDirect3D9* real;
  HRESULT hr;

  D3D9_LOG("IDirect3D9::CreateDevice hook hit\n");

  proxy = com_proxy_downcast(self);
  real = proxy->real;

  if (next_Direct3DCreate9Ex != NULL) {
    IDirect3D9Ex* d3d9ex = NULL;
    IDirect3DDevice9Ex* deviceEx = NULL;

    HRESULT result =
        IDirect3D9_QueryInterface(real, &IID_IDirect3D9Ex, (void**)&d3d9ex);
    if (SUCCEEDED(result)) {
      hr = IDirect3D9Ex_CreateDeviceEx(d3d9ex, adapter, type, hwnd, flags, pp,
                                       NULL, &deviceEx);
      IDirect3D9Ex_Release(d3d9ex);

      if (SUCCEEDED(hr)) {
        D3D9_LOG("successfully created D3D9Ex device\n");

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

  D3D9_LOG("falling back to standard D3D9 device\n");
  return IDirect3D9_CreateDevice(real, adapter, type, hwnd, flags, pp, pdev);
}

static HRESULT STDMETHODCALLTYPE my_IDirect3D9Ex_CreateDevice(
    IDirect3D9Ex* self, UINT adapter, D3DDEVTYPE type, HWND hwnd, DWORD flags,
    D3DPRESENT_PARAMETERS* pp, IDirect3DDevice9** pdev) {
  return my_IDirect3D9_CreateDevice((IDirect3D9*)self, adapter, type, hwnd,
                                    flags, pp, pdev);
}

static HRESULT STDMETHODCALLTYPE my_IDirect3D9Ex_CreateDeviceEx(
    IDirect3D9Ex* self, UINT adapter, D3DDEVTYPE type, HWND hwnd, DWORD flags,
    D3DPRESENT_PARAMETERS* pp, D3DDISPLAYMODEEX* fullscreen_display_mode,
    IDirect3DDevice9Ex** pdev) {
  struct com_proxy* proxy;
  struct com_proxy* device_proxy;
  IDirect3D9Ex* real;
  IDirect3DDevice9Ex* device;
  HRESULT hr;

  D3D9_LOG("IDirect3D9Ex::CreateDeviceEx hook hit\n");

  proxy = com_proxy_downcast(self);
  real = proxy->real;
  device = NULL;

  hr = IDirect3D9Ex_CreateDeviceEx(real, adapter, type, hwnd, flags, pp,
                                   fullscreen_display_mode, &device);
  if (FAILED(hr)) {
    return hr;
  }

  hr = com_proxy_wrap(&device_proxy, device, sizeof(IDirect3DDevice9ExVtbl));
  if (FAILED(hr)) {
    IDirect3DDevice9Ex_Release(device);
    return hr;
  }

  *pdev = (IDirect3DDevice9Ex*)device_proxy;

  return D3D_OK;
}

void d3d9_hook_unload(void) {
  if (d3d9 != NULL) {
    D3D9_LOG("unloading d3d9.dll\n");
    FreeLibrary(d3d9);
    d3d9 = NULL;
  }
}

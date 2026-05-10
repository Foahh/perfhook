#include "d3d9_hook.h"

#include <d3d9.h>
#include <stdbool.h>
#include <stdlib.h>

#include "hook/com-proxy.h"
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
static HMODULE d3d9_hook_load_real(void);

static Direct3DCreate9_t next_Direct3DCreate9;
static Direct3DCreate9Ex_t next_Direct3DCreate9Ex;
static bool d3d9_hook_initted;

static const struct hook_symbol gfx_hooks[] = {{
    .name = "Direct3DCreate9",
    .patch = Direct3DCreate9,
    .link = (void**)&next_Direct3DCreate9,
}};

static HMODULE d3d9;

static void d3d9_ensure_exports(void) {
  if (next_Direct3DCreate9 == NULL || next_Direct3DCreate9Ex == NULL) {
    d3d9_hook_load(NULL);
  }
}

static bool d3d9_pick_factory(UINT sdk_ver, IDirect3D9** out_api,
                              size_t* out_proxy_vtbl_size) {
  HRESULT hr;

  *out_api = NULL;
  *out_proxy_vtbl_size = sizeof(IDirect3D9Vtbl);

  if (next_Direct3DCreate9Ex != NULL) {
    IDirect3D9Ex* ex = NULL;
    hr = next_Direct3DCreate9Ex(sdk_ver, &ex);
    if (SUCCEEDED(hr) && ex != NULL) {
      *out_api = (IDirect3D9*)ex;
      *out_proxy_vtbl_size = sizeof(IDirect3D9ExVtbl);
      D3D9_LOG("Direct3DCreate9: wrapped IDirect3D9Ex factory\n");
      return true;
    }
    D3D9_LOG(
        "Direct3DCreate9: Direct3DCreate9Ex failed hr=0x%08lx, using "
        "Direct3DCreate9\n",
        (unsigned long)hr);
  }

  *out_api = next_Direct3DCreate9(sdk_ver);
  if (*out_api == NULL) {
    D3D9_LOG("next_Direct3DCreate9 returned NULL\n");
    return false;
  }
  D3D9_LOG("Direct3DCreate9: wrapped classic IDirect3D9 factory\n");
  return true;
}

static HRESULT d3d9_wrap_ex_device(IDirect3DDevice9Ex* device_ex,
                                   IDirect3DDevice9** pdev) {
  struct com_proxy* device_proxy;
  HRESULT hr;

  hr = com_proxy_wrap(&device_proxy, (IDirect3DDevice9*)device_ex,
                      sizeof(IDirect3DDevice9Vtbl));
  if (FAILED(hr)) {
    D3D9_LOG("CreateDevice: com_proxy_wrap(device) failed hr=0x%08lx\n",
             (unsigned long)hr);
    IDirect3DDevice9Ex_Release(device_ex);
    return hr;
  }

  *pdev = (IDirect3DDevice9*)device_proxy;
  return S_OK;
}

static HRESULT d3d9_factory_create_device_ex(IDirect3D9Ex* factory,
                                             UINT adapter, D3DDEVTYPE type,
                                             HWND hwnd, DWORD flags,
                                             D3DPRESENT_PARAMETERS* pp,
                                             IDirect3DDevice9Ex** out_dev_ex) {
  IDirect3DDevice9* dev9 = NULL;
  IDirect3DDevice9Ex* device_ex = NULL;
  HRESULT hr;
  HRESULT hr_create;

  *out_dev_ex = NULL;

  hr_create = IDirect3D9_CreateDevice((IDirect3D9*)factory, adapter, type, hwnd,
                                      flags, pp, &dev9);
  D3D9_LOG(
      "CreateDevice: IDirect3D9Ex factory IDirect3D9::CreateDevice "
      "hr=0x%08lx dev9=%p\n",
      (unsigned long)hr_create, dev9);

  hr = hr_create;
  if (SUCCEEDED(hr_create) && dev9 != NULL) {
    hr = IDirect3DDevice9_QueryInterface(dev9, &IID_IDirect3DDevice9Ex,
                                         (void**)&device_ex);
    D3D9_LOG(
        "CreateDevice: "
        "IDirect3DDevice9::QueryInterface(IID_IDirect3DDevice9Ex) "
        "hr=0x%08lx\n",
        (unsigned long)hr);
    IDirect3DDevice9_Release(dev9);
  }

  if (FAILED(hr) || device_ex == NULL) {
    if (device_ex != NULL) {
      IDirect3DDevice9Ex_Release(device_ex);
      device_ex = NULL;
    }
    hr = IDirect3D9Ex_CreateDeviceEx(factory, adapter, type, hwnd, flags, pp,
                                     NULL, &device_ex);
    D3D9_LOG(
        "CreateDevice: IDirect3D9Ex::CreateDeviceEx hr=0x%08lx pdevEx=%p\n",
        (unsigned long)hr, device_ex);
  }

  if (SUCCEEDED(hr) && device_ex != NULL) {
    *out_dev_ex = device_ex;
  } else if (device_ex != NULL) {
    IDirect3DDevice9Ex_Release(device_ex);
  }

  return hr;
}

static HRESULT d3d9_hook_create_device_dispatch(IDirect3D9* real_factory,
                                                UINT adapter, D3DDEVTYPE type,
                                                HWND hwnd, DWORD flags,
                                                D3DPRESENT_PARAMETERS* pp,
                                                IDirect3DDevice9** pdev) {
  IDirect3D9Ex* factory_ex = NULL;
  IDirect3DDevice9Ex* device_ex = NULL;
  HRESULT hr;
  HRESULT qi;

  if (next_Direct3DCreate9Ex == NULL) {
    D3D9_LOG("CreateDevice: Direct3DCreate9Ex not in d3d9.dll; skipping\n");
    goto legacy_device;
  }

  qi = IDirect3D9_QueryInterface(real_factory, &IID_IDirect3D9Ex,
                                 (void**)&factory_ex);
  D3D9_LOG(
      "CreateDevice: IDirect3D9::QueryInterface(IID_IDirect3D9Ex) "
      "hr=0x%08lx\n",
      (unsigned long)qi);

  if (FAILED(qi)) {
    D3D9_LOG("CreateDevice: QI failed - real IDirect3D9 is not IDirect3D9Ex\n");
    goto legacy_device;
  }

  hr = d3d9_factory_create_device_ex(factory_ex, adapter, type, hwnd, flags, pp,
                                     &device_ex);
  IDirect3D9Ex_Release(factory_ex);

  if (SUCCEEDED(hr) && device_ex != NULL) {
    D3D9_LOG("successfully created D3D9Ex device\n");
    return d3d9_wrap_ex_device(device_ex, pdev);
  }

legacy_device:
  D3D9_LOG("falling back to standard D3D9 device (IDirect3D9::CreateDevice)\n");
  return IDirect3D9_CreateDevice(real_factory, adapter, type, hwnd, flags, pp,
                                 pdev);
}

void d3d9_hook_load(HINSTANCE self) {
  if (next_Direct3DCreate9 == NULL || next_Direct3DCreate9Ex == NULL) {
    d3d9 = d3d9_hook_load_real();

    if (d3d9 == NULL && next_Direct3DCreate9 == NULL) {
      D3D9_LOG("d3d9.dll not found or failed initialization\n");
      goto fail;
    }

    if (d3d9 != NULL) {
      if (next_Direct3DCreate9 == NULL) {
        next_Direct3DCreate9 =
            (Direct3DCreate9_t)GetProcAddress(d3d9, "Direct3DCreate9");
      }
      if (next_Direct3DCreate9Ex == NULL) {
        next_Direct3DCreate9Ex =
            (Direct3DCreate9Ex_t)GetProcAddress(d3d9, "Direct3DCreate9Ex");
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
  size_t factory_vtbl_size;

  D3D9_LOG("Direct3DCreate9 hook hit\n");

  d3d9_ensure_exports();
  if (next_Direct3DCreate9 == NULL) {
    D3D9_LOG("Direct3DCreate9 unavailable\n");
    return NULL;
  }

  if (!d3d9_pick_factory(sdk_ver, &api, &factory_vtbl_size)) {
    return NULL;
  }

  hr = com_proxy_wrap(&proxy, api, factory_vtbl_size);
  if (FAILED(hr)) {
    D3D9_LOG("com_proxy_wrap returned %x\n", (int)hr);
    IDirect3D9_Release(api);
    return NULL;
  }

  vtbl = proxy->vptr;
  vtbl->CreateDevice = my_IDirect3D9_CreateDevice;

  return (IDirect3D9*)proxy;
}

static HRESULT STDMETHODCALLTYPE my_IDirect3D9_CreateDevice(
    IDirect3D9* self, UINT adapter, D3DDEVTYPE type, HWND hwnd, DWORD flags,
    D3DPRESENT_PARAMETERS* pp, IDirect3DDevice9** pdev) {
  struct com_proxy* proxy;
  IDirect3D9* real;

  D3D9_LOG(
      "IDirect3D9::CreateDevice hook hit adapter=%u devtype=%u hwnd=%p "
      "behavior=0x%lx backbuf=%ux%u\n",
      adapter, (unsigned)type, hwnd, (unsigned long)flags,
      pp != NULL ? (unsigned)pp->BackBufferWidth : 0u,
      pp != NULL ? (unsigned)pp->BackBufferHeight : 0u);

  proxy = com_proxy_downcast(self);
  real = proxy->real;

  return d3d9_hook_create_device_dispatch(real, adapter, type, hwnd, flags, pp,
                                          pdev);
}

void d3d9_hook_unload(void) {
  if (d3d9 != NULL) {
    D3D9_LOG("unloading d3d9.dll\n");
    FreeLibrary(d3d9);
    d3d9 = NULL;
  }
}

#include "nvidia.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <windows.h>

// clang-format off
#include "nvapi/nvapi.h"
#include "nvapi/NvApiDriverSettings.h"
// clang-format on

#include "util/dprintf.h"

#define NVAPI_LOG(...) dprintf("nvapi: " __VA_ARGS__)

static HMODULE nvapi = NULL;
static bool nvapi_ready = false;

/*
 * Function pointer typedefs from the original code.
 */
typedef uintptr_t *(*NvAPI_QueryInterface_t)(unsigned int);
typedef NvAPI_Status (*NvAPI_Initialize_t)();
typedef NvAPI_Status (*NvAPI_Unload_t)();
typedef NvAPI_Status (*NvAPI_GetErrorMessage_t)(NvAPI_Status,
                                                NvAPI_ShortString);
typedef NvAPI_Status (*NvAPI_DRS_CreateSession_t)(NvDRSSessionHandle *);
typedef NvAPI_Status (*NvAPI_DRS_LoadSettings_t)(NvDRSSessionHandle);
typedef NvAPI_Status (*NvAPI_DRS_FindProfileByName_t)(NvDRSSessionHandle,
                                                      NvAPI_UnicodeString,
                                                      NvDRSProfileHandle *);
typedef NvAPI_Status (*NvAPI_DRS_FindApplicationByName_t)(NvDRSSessionHandle,
                                                          NvAPI_UnicodeString,
                                                          NvDRSProfileHandle *,
                                                          NVDRS_APPLICATION *);
typedef NvAPI_Status (*NvAPI_DRS_CreateProfile_t)(NvDRSSessionHandle,
                                                  NVDRS_PROFILE *,
                                                  NvDRSProfileHandle *);
typedef NvAPI_Status (*NvAPI_DRS_CreateApplication_t)(NvDRSSessionHandle,
                                                      NvDRSProfileHandle,
                                                      NVDRS_APPLICATION *);
typedef NvAPI_Status (*NvAPI_DRS_GetSetting_t)(NvDRSSessionHandle,
                                               NvDRSProfileHandle, NvU32,
                                               NVDRS_SETTING *);
typedef NvAPI_Status (*NvAPI_DRS_SetSetting_t)(NvDRSSessionHandle,
                                               NvDRSProfileHandle,
                                               NVDRS_SETTING *);
typedef NvAPI_Status (*NvAPI_DRS_SaveSettings_t)(NvDRSSessionHandle);
typedef NvAPI_Status (*NvAPI_DRS_DestroySession_t)(NvDRSSessionHandle);

/*
 * Global function pointers referring to NVAPI functions.
 */
static NvAPI_QueryInterface_t NvAPI_QueryInterface_f = NULL;
static NvAPI_Initialize_t NvAPI_Initialize_f = NULL;
static NvAPI_Unload_t NvAPI_Unload_f = NULL;
static NvAPI_GetErrorMessage_t NvAPI_GetErrorMessage_f = NULL;
static NvAPI_DRS_CreateSession_t NvAPI_DRS_CreateSession_f = NULL;
static NvAPI_DRS_LoadSettings_t NvAPI_DRS_LoadSettings_f = NULL;
static NvAPI_DRS_FindProfileByName_t NvAPI_DRS_FindProfileByName_f = NULL;
static NvAPI_DRS_FindApplicationByName_t NvAPI_DRS_FindApplicationByName_f =
    NULL;
static NvAPI_DRS_CreateProfile_t NvAPI_DRS_CreateProfile_f = NULL;
static NvAPI_DRS_CreateApplication_t NvAPI_DRS_CreateApplication_f = NULL;
static NvAPI_DRS_GetSetting_t NvAPI_DRS_GetSetting_f = NULL;
static NvAPI_DRS_SetSetting_t NvAPI_DRS_SetSetting_f = NULL;
static NvAPI_DRS_SaveSettings_t NvAPI_DRS_SaveSettings_f = NULL;
static NvAPI_DRS_DestroySession_t NvAPI_DRS_DestroySession_f = NULL;

static void nvapi_clear_functions(void) {
  NvAPI_QueryInterface_f = NULL;
  NvAPI_Initialize_f = NULL;
  NvAPI_Unload_f = NULL;
  NvAPI_GetErrorMessage_f = NULL;
  NvAPI_DRS_CreateSession_f = NULL;
  NvAPI_DRS_LoadSettings_f = NULL;
  NvAPI_DRS_FindProfileByName_f = NULL;
  NvAPI_DRS_FindApplicationByName_f = NULL;
  NvAPI_DRS_CreateProfile_f = NULL;
  NvAPI_DRS_CreateApplication_f = NULL;
  NvAPI_DRS_GetSetting_f = NULL;
  NvAPI_DRS_SetSetting_f = NULL;
  NvAPI_DRS_SaveSettings_f = NULL;
  NvAPI_DRS_DestroySession_f = NULL;
}

char *get_error_message(NvAPI_Status status) {
  static NvAPI_ShortString sz_desc;
  memset(sz_desc, 0, sizeof(sz_desc));

  if (NvAPI_GetErrorMessage_f != NULL) {
    NvAPI_GetErrorMessage_f(status, sz_desc);
  }
  return sz_desc;
}

void nvapi_load(void) {
  bool initialized;

  initialized = false;
  nvapi_ready = false;
  nvapi_clear_functions();

  NVAPI_LOG("Loading nvapi.dll...\n");
  nvapi = LoadLibraryW(L"nvapi.dll");

  if (nvapi == NULL) {
    NVAPI_LOG("NVAPI DLL not available\n");
    return;
  }
  NVAPI_LOG("NVAPI DLL loaded.\n");

  NVAPI_LOG("Find address of nvapi_QueryInterface...\n");
  NvAPI_QueryInterface_f =
      (NvAPI_QueryInterface_t)GetProcAddress(nvapi, "nvapi_QueryInterface");
  if (!NvAPI_QueryInterface_f) {
    NVAPI_LOG("nvapi_QueryInterface not found\n");
    goto fail;
  }

  NVAPI_LOG("Calling NvAPI_Initialize...\n");
  NvAPI_Initialize_f = (NvAPI_Initialize_t)NvAPI_QueryInterface_f(0x0150E828);
  if (!NvAPI_Initialize_f) {
    NVAPI_LOG("NvAPI_Initialize not found\n");
    goto fail;
  }

  // Check if NVAPI is already initialized
  NvAPI_Status status = NvAPI_Initialize_f();
  if (status != NVAPI_OK) {
    NVAPI_LOG("NvAPI_Initialize failed: %s\n", get_error_message(status));
    goto fail;
  }
  initialized = true;
  NVAPI_LOG("NvAPI_Initialize succeeded\n");

  NvAPI_Unload_f = (NvAPI_Unload_t)NvAPI_QueryInterface_f(0xD22BDD7E);
  if (!NvAPI_Unload_f) {
    NVAPI_LOG("NvAPI_Unload not found\n");
    goto fail;
  }
  NvAPI_GetErrorMessage_f =
      (NvAPI_GetErrorMessage_t)NvAPI_QueryInterface_f(0x6C2D048C);
  if (!NvAPI_GetErrorMessage_f) {
    NVAPI_LOG("NvAPI_GetErrorMessage not found\n");
    goto fail;
  }
  NvAPI_DRS_CreateSession_f =
      (NvAPI_DRS_CreateSession_t)NvAPI_QueryInterface_f(0x0694D52E);
  if (!NvAPI_DRS_CreateSession_f) {
    NVAPI_LOG("NvAPI_DRS_CreateSession not found\n");
    goto fail;
  }
  NvAPI_DRS_LoadSettings_f =
      (NvAPI_DRS_LoadSettings_t)NvAPI_QueryInterface_f(0x375DBD6B);
  if (!NvAPI_DRS_LoadSettings_f) {
    NVAPI_LOG("NvAPI_DRS_LoadSettings not found\n");
    goto fail;
  }
  NvAPI_DRS_FindProfileByName_f =
      (NvAPI_DRS_FindProfileByName_t)NvAPI_QueryInterface_f(0x7E4A9A0B);
  if (!NvAPI_DRS_FindProfileByName_f) {
    NVAPI_LOG("NvAPI_DRS_FindProfileByName not found\n");
    goto fail;
  }
  NvAPI_DRS_FindApplicationByName_f =
      (NvAPI_DRS_FindApplicationByName_t)NvAPI_QueryInterface_f(0xEEE566B2);
  if (!NvAPI_DRS_FindApplicationByName_f) {
    NVAPI_LOG("NvAPI_DRS_FindApplicationByName not found\n");
    goto fail;
  }
  NvAPI_DRS_CreateProfile_f =
      (NvAPI_DRS_CreateProfile_t)NvAPI_QueryInterface_f(0x0CC176068);
  if (!NvAPI_DRS_CreateProfile_f) {
    NVAPI_LOG("NvAPI_DRS_CreateProfile not found\n");
    goto fail;
  }
  NvAPI_DRS_CreateApplication_f =
      (NvAPI_DRS_CreateApplication_t)NvAPI_QueryInterface_f(0x4347A9DE);
  if (!NvAPI_DRS_CreateApplication_f) {
    NVAPI_LOG("NvAPI_DRS_CreateApplication not found\n");
    goto fail;
  }
  NvAPI_DRS_GetSetting_f =
      (NvAPI_DRS_GetSetting_t)NvAPI_QueryInterface_f(0x73BF8338);
  if (!NvAPI_DRS_GetSetting_f) {
    NVAPI_LOG("NvAPI_DRS_GetSetting not found\n");
    goto fail;
  }
  NvAPI_DRS_SetSetting_f =
      (NvAPI_DRS_SetSetting_t)NvAPI_QueryInterface_f(0x577DD202);
  if (!NvAPI_DRS_SetSetting_f) {
    NVAPI_LOG("NvAPI_DRS_SetSetting not found\n");
    goto fail;
  }
  NvAPI_DRS_SaveSettings_f =
      (NvAPI_DRS_SaveSettings_t)NvAPI_QueryInterface_f(0xFCBC7E14);
  if (!NvAPI_DRS_SaveSettings_f) {
    NVAPI_LOG("NvAPI_DRS_SaveSettings not found\n");
    goto fail;
  }
  NvAPI_DRS_DestroySession_f =
      (NvAPI_DRS_DestroySession_t)NvAPI_QueryInterface_f(0x0DAD9CFF8);
  if (!NvAPI_DRS_DestroySession_f) {
    NVAPI_LOG("NvAPI_DRS_DestroySession not found\n");
    goto fail;
  }

  nvapi_ready = true;
  NVAPI_LOG("NVAPI module initialized\n");

  return;

fail:
  if (initialized && NvAPI_Unload_f != NULL) {
    NvAPI_Unload_f();
  }
  if (nvapi != NULL) {
    FreeLibrary(nvapi);
    nvapi = NULL;
  }
  nvapi_clear_functions();
  nvapi_ready = false;
}

NvAPI_Status create_profile_if_needed(NvDRSSessionHandle h_session,
                                      NvDRSProfileHandle *h_profile) {
  NvAPI_UnicodeString app_name = L"chusanApp.exe";

  NVDRS_APPLICATION application;
  memset(&application, 0, sizeof(application));
  application.version = NVDRS_APPLICATION_VER;

  // Find the application profile
  NvAPI_Status status1 = NvAPI_DRS_FindApplicationByName_f(
      h_session, app_name, h_profile, &application);
  if (status1 == NVAPI_OK) {
    return status1;
  } else if (status1 != NVAPI_EXECUTABLE_NOT_FOUND) {
    NVAPI_LOG("error finding application profile: %s\n",
              get_error_message(status1));
    return status1;
  }

  // Create a new application profile
  NVDRS_PROFILE profile;
  memset(&profile, 0, sizeof(profile));
  profile.version = NVDRS_PROFILE_VER;
  memcpy_s(profile.profileName, sizeof(profile.profileName), L"chusanApp",
           10 * sizeof(wchar_t));
  NvAPI_Status status2 =
      NvAPI_DRS_FindProfileByName_f(h_session, profile.profileName, h_profile);
  if (status2 == NVAPI_PROFILE_NOT_FOUND) {
    NVAPI_LOG("creating driver profile\n");

    NVDRS_GPU_SUPPORT gpu_support;
    memset(&gpu_support, 0, sizeof(gpu_support));
    gpu_support.nvs = 1;
    profile.gpuSupport = gpu_support;

    status2 = NvAPI_DRS_CreateProfile_f(h_session, &profile, h_profile);
    if (status2 != NVAPI_OK && status2 != NVAPI_PROFILE_NAME_IN_USE) {
      NVAPI_LOG("could not create driver profile: %s\n",
                get_error_message(status2));
      return status2;
    }
  } else if (status2 != NVAPI_OK) {
    NVAPI_LOG("error finding driver profile: %s\n", get_error_message(status2));
    return status2;
  }

  // Create the application profile
  NVAPI_LOG("creating application profile\n");
  memcpy_s(application.appName, sizeof(application.appName), L"chusanApp.exe",
           14 * sizeof(wchar_t));
  memcpy_s(application.userFriendlyName, sizeof(application.userFriendlyName),
           L"chusanApp", 10 * sizeof(wchar_t));

  NvAPI_Status status3 =
      NvAPI_DRS_CreateApplication_f(h_session, *h_profile, &application);
  if (status3 != NVAPI_OK) {
    NVAPI_LOG("could not create application profile: %s\n",
              get_error_message(status3));
    return status3;
  }

  return NVAPI_OK;
}

NvAPI_Status set_gpu_power_state(NvDRSSessionHandle h_session,
                                 NvDRSProfileHandle h_profile) {
  NVDRS_SETTING drs_setting;
  memset(&drs_setting, 0, sizeof(drs_setting));
  drs_setting.version = NVDRS_SETTING_VER;
  drs_setting.settingId = PREFERRED_PSTATE_ID;
  drs_setting.settingType = NVDRS_DWORD_TYPE;
  drs_setting.u32PredefinedValue = PREFERRED_PSTATE_PREFER_MAX;
  drs_setting.u32CurrentValue = PREFERRED_PSTATE_PREFER_MAX;

  return NvAPI_DRS_SetSetting_f(h_session, h_profile, &drs_setting);
}

NvAPI_Status set_vsync_mode(NvDRSSessionHandle h_session,
                            NvDRSProfileHandle h_profile) {
  NVDRS_SETTING drs_setting;
  memset(&drs_setting, 0, sizeof(drs_setting));
  drs_setting.version = NVDRS_SETTING_VER;
  drs_setting.settingId = VSYNCMODE_ID;
  drs_setting.settingType = NVDRS_DWORD_TYPE;
  drs_setting.u32PredefinedValue = VSYNCMODE_PASSIVE;
  drs_setting.u32CurrentValue = VSYNCMODE_PASSIVE;

  return NvAPI_DRS_SetSetting_f(h_session, h_profile, &drs_setting);
}

void nvapi_set_profile_settings(void) {
  if (!nvapi_ready) {
    NVAPI_LOG("NVAPI not initialized\n");
    return;
  }

  NvDRSSessionHandle h_session = NULL;
  NvAPI_Status status = NvAPI_DRS_CreateSession_f(&h_session);

  NVAPI_LOG("creating driver settings session (DRS)...\n");
  if (status != NVAPI_OK) {
    NVAPI_LOG("could not create driver settings session: %s\n",
              get_error_message(status));
    return;
  }

  NVAPI_LOG("loading driver settings...\n");
  status = NvAPI_DRS_LoadSettings_f(h_session);
  if (status != NVAPI_OK) {
    NVAPI_LOG("could not load driver settings: %s\n",
              get_error_message(status));
    NvAPI_DRS_DestroySession_f(h_session);
    return;
  }

  NvDRSProfileHandle h_profile = NULL;
  NVAPI_LOG("creating NVIDIA profile for chusanApp.exe...\n");
  if (create_profile_if_needed(h_session, &h_profile) != NVAPI_OK) {
    NvAPI_DRS_DestroySession_f(h_session);
    return;
  }

  // Set the application profile settings
  NVAPI_LOG("setting VSync to Application Controlled...\n");
  status = set_vsync_mode(h_session, h_profile);
  if (status != NVAPI_OK) {
    NVAPI_LOG("could not set VSync mode: %s\n", get_error_message(status));
    NvAPI_DRS_DestroySession_f(h_session);
    return;
  }

  NVAPI_LOG("applying preferred PState to Maximum Performance...\n");
  status = set_gpu_power_state(h_session, h_profile);
  if (status != NVAPI_OK) {
    NVAPI_LOG("could not set preferred PState: %s\n",
              get_error_message(status));
    NvAPI_DRS_DestroySession_f(h_session);
    return;
  }

  NVAPI_LOG("saving settings for DRS session...\n");
  status = NvAPI_DRS_SaveSettings_f(h_session);
  if (status != NVAPI_OK) {
    NVAPI_LOG("could not save driver settings: %s\n",
              get_error_message(status));
    NvAPI_DRS_DestroySession_f(h_session);
    return;
  }

  NVAPI_LOG("destroying DRS session...\n");
  status = NvAPI_DRS_DestroySession_f(h_session);
  if (status != NVAPI_OK) {
    NVAPI_LOG("failed to destroy driver session: %s\n",
              get_error_message(status));
  }
}

void nvapi_unload(void) {
  if (nvapi != NULL) {
    NVAPI_LOG("unloading nvapi.dll...\n");
    if (nvapi_ready && NvAPI_Unload_f != NULL) {
      NvAPI_Unload_f();
    }
    FreeLibrary(nvapi);
    nvapi = NULL;
  }
  nvapi_ready = false;
  nvapi_clear_functions();
}

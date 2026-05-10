shared_library(
    'perfhook',
    name_prefix: '',
    include_directories: inc,
    implicit_include_directories: false,
    dependencies: [
        hook_dep,
        hooklib_dep,
        dprintf_dep,
        nvapi_dep,
        dxguid_lib,
    ],
    link_with: [
        util_lib,
        nvidia_lib,
        d3d9_hook_lib,
    ],
    sources: [
        'dllmain.c',
    ],
)
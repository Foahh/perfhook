d3d9_hook_lib = static_library(
    'd3d9_hook',
    include_directories: inc,
    implicit_include_directories: false,
    dependencies: [
        hook_dep,
        hooklib_dep,
        dprintf_dep,
        dxguid_lib,
    ],
    sources: [
        'd3d9_hook.c',
        'd3d9_hook.h',
    ],
)

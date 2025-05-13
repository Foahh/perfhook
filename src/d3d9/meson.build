d3d9_lib = static_library(
    'd3d9',
    include_directories: inc,
    implicit_include_directories: false,
    dependencies: [
        hook_dep,
        hooklib_dep,
        dprintf_dep,
        dxguid_lib,
        shlwapi_lib,
    ],
    sources: [
        'd3d9.c',
        'd3d9.h',
    ],
)
nvidia_lib = static_library(
    'nvidia',
    include_directories: inc,
    implicit_include_directories: false,
    dependencies: [
        dprintf_dep,
        nvapi_dep,
    ],
    sources: [
        'nvidia.c',
        'nvidia.h',
    ],
)
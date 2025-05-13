d3d9_lib = static_library(
    'd3d9',
    include_directories : inc,
    implicit_include_directories : false,
    dependencies : [
        capnhook.get_variable('hook_dep'),
        dxguid_lib
    ],
    link_with : [
        hooklib_lib,
        util_lib,
    ],
    sources : [
        'd3d9.c',
        'd3d9.h',
    ],
)

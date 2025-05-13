shared_library(
    'graphic_hook',
    name_prefix : '',
    include_directories : inc,
    implicit_include_directories : false,
    dependencies : [
        capnhook.get_variable('hook_dep'),
        dxguid_lib
    ],
    link_with : [
        hooklib_lib,
        util_lib,
        nvidia_lib,
        d3d9_lib,
    ],
    sources : [
        'dllmain.c',
    ],
)

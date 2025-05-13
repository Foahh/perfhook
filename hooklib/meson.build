hooklib_lib = static_library(
    'hooklib',
    include_directories : inc,
    implicit_include_directories : false,
    dependencies : [
        capnhook.get_variable('hook_dep'),
    ],
    sources : [
        'dll.c',
        'dll.h',
    ],
)

project('segatools-common', 'c')

capnhook = subproject('capnhook')
segatools_hook_dep = capnhook.get_variable('hook_dep')
inc = include_directories('common')

segatools_dprintf_lib = static_library(
    'segatools-dprintf',
    include_directories: inc,
    implicit_include_directories: false,
    sources: [
        'common/util/dprintf.c',
        'common/util/dprintf.h',
    ],
)

segatools_dprintf_dep = declare_dependency(
    include_directories: inc,
    link_with: segatools_dprintf_lib,
)

segatools_hooklib_lib = static_library(
    'segatools-hooklib',
    include_directories: inc,
    implicit_include_directories: false,
    dependencies: [
        segatools_hook_dep,
    ],
    sources: [
        'common/hooklib/dll.c',
        'common/hooklib/dll.h',
    ],
)

segatools_hooklib_dep = declare_dependency(
    include_directories: inc,
    dependencies: [
        segatools_hook_dep,
    ],
    link_with: segatools_hooklib_lib,
)

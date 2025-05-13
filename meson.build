project(
    'graphic_hook',
    'c',
    version: '1.0.0',
    default_options: [
        'werror=true',
    ],
)

add_project_arguments(
    '-DCOBJMACROS',
    '-DDIRECTINPUT_VERSION=0x0800',
    '-DWIN32_LEAN_AND_MEAN',
    '-D_WIN32_WINNT=_WIN32_WINNT_WIN10',
    '-DMINGW_HAS_SECURE_API=1',
    language: 'c',
)

cc = meson.get_compiler('c')

if cc.get_id() != 'msvc'
    add_project_arguments(
        '-ffunction-sections',
        '-fdata-sections',
        '-Wno-unused',
        language: 'c',
    )

    add_project_link_arguments(
        '-Wl,--enable-stdcall-fixup',
        '-Wl,--exclude-all-symbols',
        '-Wl,--gc-sections',
        '-static-libgcc',
        language: 'c',
    )
endif

if cc.get_id() == 'msvc'
    add_project_arguments(
        '/wd4819',
        language: 'c',
    )
endif

dxguid_lib = cc.find_library('dxguid')

inc = include_directories('.')
capnhook = subproject('capnhook')

subdir('util')
subdir('hooklib')
subdir('lib/d3d9')
subdir('lib/nvidia')

subdir('main')

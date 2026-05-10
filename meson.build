project(
    'perfhook',
    'c',
    version: '1.0.0',
    default_options: [
        'werror=true',
    ],
    subproject_dir: 'third_party',
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
inc = include_directories('.')

dxguid_lib = cc.find_library('dxguid')
shlwapi_lib = cc.find_library('shlwapi')

segatools = subproject('segatools')
hook_dep = segatools.get_variable('segatools_hook_dep')
hooklib_dep = segatools.get_variable('segatools_hooklib_dep')
dprintf_dep = segatools.get_variable('segatools_dprintf_dep')

nvapi = subproject('nvapi')
nvapi_dep = nvapi.get_variable('nvapi_dep')

subdir('src/util')
subdir('src/d3d9_hook')
subdir('src/nvidia')

subdir('src')

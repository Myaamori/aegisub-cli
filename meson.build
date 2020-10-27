project('Aegisub-CLI', ['c', 'cpp'],
        license: 'BSD-3-Clause',
        meson_version: '>=0.55.0',
        # c_std=c11 should be here but it breaks Linux builds for some reason, needs investigation
        default_options: ['cpp_std=c++11', 'b_lto=true'],
        version: '3.2.2')

if host_machine.system() == 'windows'
    version_sh = find_program('tools/version.ps1')
else
    version_sh = find_program('tools/version.sh')
endif
version_inc = include_directories('.')
version_h = custom_target('git_version.h',
                          command: [version_sh, meson.source_root(), '@OUTDIR@'],
                          build_by_default: true,
                          build_always_stale: true, # has internal check whether target file will be refreshed
                          output: ['git_version.h', 'git_version.xml'])

dataroot = get_option('prefix') / get_option('datadir') / 'aegisub'
add_project_arguments('-DP_DATA="@0@"'.format(dataroot), language: 'cpp')

if host_machine.system() == 'windows'
    add_project_arguments('-DWIN32', '-DNOMINMAX', '-D_WIN32_WINNT=0x0602', language: 'cpp')
    add_project_arguments('-DWIN32', '-DNOMINMAX', '-D_WIN32_WINNT=0x0602', language: 'c')
endif

conf = configuration_data()
conf.set_quoted('P_DATA', dataroot)
if get_option('credit') != ''
    conf.set_quoted('BUILD_CREDIT', get_option('credit'))
endif

deps = []
deps_inc = []

conf.set('WITH_FONTCONFIG', '0')
if host_machine.system() == 'darwin'
    add_languages('objc', 'objcpp')
    add_project_arguments('-DGL_SILENCE_DEPRECATION', language: 'cpp')
    # meson does not currently support objcpp_std
    add_project_arguments('-std=c++11', language: 'objcpp')
elif host_machine.system() != 'windows'
    conf.set('WITH_FONTCONFIG', '1')
    deps += dependency('fontconfig')
endif

cxx = meson.get_compiler('cpp')
cc = meson.get_compiler('c')
deps += cc.find_library('m', required: false)
deps += cc.find_library('dl', required: false)
if not cxx.check_header('iconv.h')
    iconv_sp = subproject('iconv') # this really needs to be replaced with a proper port
    deps += iconv_sp.get_variable('libiconv_dep')
endif

boost_modules = ['chrono', 'filesystem', 'thread', 'locale', 'regex', 'program_options']
if not get_option('local_boost')
    boost_dep = dependency('boost', version: '>=1.50.0',
                            modules: boost_modules,
                            required: cxx.get_id() != 'msvc',
                            static: get_option('default_library') == 'static')
endif

if get_option('local_boost') or not boost_dep.found()
    boost_dep = []
    boost = subproject('boost')
    foreach module: boost_modules
        boost_dep += boost.get_variable('boost_' + module + '_dep')
    endforeach
endif

deps += boost_dep
if host_machine.system() == 'windows'
    conf.set('BOOST_USE_WINDOWS_H', '1')
endif

deps += dependency('zlib')

add_project_arguments('-DUNICODE', '-D_UNICODE', language: 'cpp')

if host_machine.system() != 'windows'
    wx_dep = dependency('wxWidgets', version: '>=3.0.0',
                        required: cxx.get_id() != 'msvc',
                        modules: ['std'])
    if not wx_dep.found() # this will only be hit with msvc
        if get_option('wx_path') != ''
            deps_inc += include_directories(get_option('wx_path') / 'include' / 'msvc', get_option('wx_path') / 'include')
            add_project_arguments('-DwxMSVC_VERSION_AUTO', '-DWXUSINGDLL', language: 'cpp')
        else
            error('wxWidgets not found and no wx_path defined.')
        endif
    endif
deps += wx_dep
endif

deps += dependency('icu-uc', version: '>=4.8.1.1')
deps += dependency('icu-i18n', version: '>=4.8.1.1')
deps += dependency('ffms2')
conf.set('WITH_FFMS2', '1')

if host_machine.system() == 'darwin'
    frameworks_dep = dependency('appleframeworks', modules : ['CoreText', 'CoreFoundation'])
    deps += frameworks_dep
endif

# TODO: OSS

conf_platform = configuration_data()
conf_platform.set('DEFAULT_PLAYER_AUDIO', '')

luajit_sp = subproject('luajit')
luajit_inc = luajit_sp.get_variable('incdir')
deps += luajit_sp.get_variable('luajit_dep')

subdir('subprojects/luabins/src')

# TODO: csri

acconf = configure_file(output: 'acconf.h', configuration: conf)

if get_option('include_automations')
    subdir('automation')
endif
subdir('libaegisub')
subdir('src')

project(
  'log_appenders',
  'cpp',
  version: '0.1.0',
  default_options: [
    'warning_level=3',
    'cpp_std=c++17',
    'libdir=lib',
    'cpp_args=-fPIC -Wall -Wno-unused-parameter -Wno-unused-variable -Wno-sign-compare -Wno-pedantic',
    'default_library=shared',
  ],
)

cpp_compiler = meson.get_compiler('cpp')
fs = import('fs')
add_project_arguments('-fvisibility=hidden', language: 'cpp')
build_tests = get_option('tests')
plugin_install_dir = get_option('plugin_install_dir')
enable_conan_compile = get_option('enable_conan_compile')

# 优先探测外部 liblogger/liblogging；探测不到时回退到工程内 stub。
liblogger_optional_dep = dependency('liblogger', method: 'pkg-config', required: false)
if not liblogger_optional_dep.found()
  liblogger_optional_dep = dependency('liblogger', required: false)
endif

liblogging_filename = 'liblogging.so'
if host_machine.system() == 'darwin'
  liblogging_filename = 'liblogging.dylib'
endif

liblogging_path = ''
external_logging_dep = declare_dependency()
if liblogger_optional_dep.found()
  liblogger_libdir = liblogger_optional_dep.get_variable(pkgconfig: 'libdir', default_value: '')
  if liblogger_libdir == ''
    liblogger_libdir = liblogger_optional_dep.get_variable('libdir', default_value: '')
  endif

  if liblogger_libdir != ''
    liblogging_candidate = join_paths(liblogger_libdir, liblogging_filename)
    if fs.exists(liblogging_candidate)
      liblogging_path = liblogging_candidate
      external_logging_dep = liblogger_optional_dep
      message('log_appenders 检测到外部 logging 库: ' + liblogging_path)
    endif
  endif
endif

if liblogging_path == '' and (enable_conan_compile or meson.is_cross_build())
  current_buildtype = get_option('buildtype')
  if current_buildtype == 'debug'
    add_project_arguments('-DBUILD_TYPE=BUILD_TYPE_DEBUG', language: 'cpp')
  else
    add_project_arguments('-DBUILD_TYPE=BUILD_TYPE_RELEASE', language: 'cpp')
  endif
  add_project_arguments('-DLOGGING_PATH="/usr/lib64/liblogging.so"', language: 'cpp')
elif liblogging_path != ''
  add_project_arguments('-DBUILD_TYPE=BUILD_TYPE_DT', language: 'cpp')
  add_project_arguments('-DLOGGING_PATH="' + liblogging_path + '"', language: 'cpp')
else
  add_project_arguments('-DBUILD_TYPE=BUILD_TYPE_DT', language: 'cpp')
  add_project_arguments('-DLOGGING_PATH=""', language: 'cpp')
  message('log_appenders 未检测到外部 logging 库，回退到工程内 stub 实现')
endif

mcbase_dep = dependency(
  'mcbase',
  fallback: ['mcbase', 'mcbase_dep'],
  default_options: [
    'default_library=shared',
    'tests_utilities=' + build_tests.to_string(),
  ],
  required: true,
)
mcengine_dep = dependency(
  'mcengine',
  fallback: ['mcengine', 'mcengine_dep'],
  default_options: ['default_library=shared'],
  required: true,
)
glib_dep = dependency('glib-2.0', method: 'pkg-config', required: true)

log_appenders_inc = include_directories('include')
log_appenders_private_inc = include_directories('src/include')
incs = [log_appenders_inc, log_appenders_private_inc]

# 使用 shared_library（而非 shared_module），以便单测可链接具体符号；运行时仍由 appender_factory dlopen 加载。
common_link_args = []
if cpp_compiler.get_id() == 'clang' and host_machine.system() == 'darwin'
  common_link_args += ['-undefined', 'dynamic_lookup']
endif

file_appender_mod = shared_library(
  'file_appender',
  'src/file_appender.cpp',
  include_directories: incs,
  dependencies: [mcbase_dep, mcengine_dep, glib_dep, external_logging_dep],
  gnu_symbol_visibility: 'default',
  name_prefix: '',
  install: true,
  install_dir: plugin_install_dir,
  link_args: common_link_args,
)

socket_appender_mod = shared_library(
  'socket_appender',
  ['src/socket_appender.cpp', 'src/socket_client.cpp'],
  include_directories: incs,
  dependencies: [mcbase_dep, glib_dep, external_logging_dep],
  gnu_symbol_visibility: 'default',
  name_prefix: '',
  install: true,
  install_dir: plugin_install_dir,
  link_args: common_link_args,
)

modules_out_dir = meson.current_build_dir()

appenders_plugins_dep = declare_dependency(
  link_with: [file_appender_mod, socket_appender_mod],
  include_directories: [log_appenders_inc],
  dependencies: [mcbase_dep, mcengine_dep, glib_dep, external_logging_dep],
)

log_appenders_dep = declare_dependency(
  include_directories: [log_appenders_inc],
  dependencies: [mcbase_dep, mcengine_dep, glib_dep, external_logging_dep],
)

if build_tests
  gtest_dep = dependency('gtest', method: 'pkg-config', required: true)
  mcbase_subproject = subproject('mcbase', default_options: [
    'default_library=shared',
    'tests_utilities=true',
  ])
  mcbase_test_utilities_dep = mcbase_subproject.get_variable('test_utilities_dep')
  subdir('tests')
endif

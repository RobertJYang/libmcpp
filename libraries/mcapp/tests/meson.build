mcapp_test_files = files(
  'test_app_proto.cpp',
  'test_application.cpp',
  'test_base_app.cpp',
  'test_bus_name_invalid.cpp',
  'test_dbus_proto.cpp',
  'test_micro_component.cpp',
  'test_proxy.cpp',
  'test_service_registry.cpp',
)

mcapp_test_main = configure_file(
  input: files('main.cpp.in'),
  output: 'main.cpp',
  copy: true,
)

mcapp_test_cpp_args = []
mcapp_test_link_args = []
if enable_coverage
  mcapp_test_cpp_args += coverage_args
  mcapp_test_link_args += coverage_link_args
endif

mcapp_test_lib = static_library(
  'mcapp_test_lib',
  mcapp_test_files,
  include_directories: [incdir, srcdir, mcapp_test_incdir],
  dependencies: [
    gtest_dep,
    mcapp_dep,
    mcengine_dep,
    mcapp_test_utilities_dep,
  ],
  install: false,
  build_by_default: false,
)

mcapp_test_exe = executable(
  'mcapp_test',
  mcapp_test_main,
  link_whole: [
    mcapp_test_lib,
  ],
  include_directories: [incdir, srcdir, mcapp_test_incdir],
  dependencies: [
    gtest_dep,
    mcapp_test_utilities_dep,
    mcapp_dep,
    mcengine_dep,
  ],
  cpp_args: mcapp_test_cpp_args,
  link_args: mcapp_test_link_args,
  install: false,
  build_by_default: false,
)

mcapp_test_env = environment()
mcapp_test_env.set('MC_BUILD_ROOT', meson.project_build_root())
mcapp_test_env.set('MC_GLOBAL_BUILD_ROOT', meson.global_build_root())

# libsomp.so 依赖 liblogging.so，需要将其 lib 目录加入搜索路径
# 优先使用 pkg-config，失败则回退到默认查找方式（支持 Conan CMake 等）
_liblogger_dep = dependency('liblogger', method: 'pkg-config', required: false)
if not _liblogger_dep.found()
  _liblogger_dep = dependency('liblogger', required: false)
endif
if _liblogger_dep.found()
  _liblogger_libdir = _liblogger_dep.get_variable(pkgconfig: 'libdir', default_value: '')
  if _liblogger_libdir == ''
    _liblogger_libdir = _liblogger_dep.get_variable('libdir', default_value: '')
  endif
  if _liblogger_libdir != ''
    message('liblogger libdir: ' + _liblogger_libdir)
    mcapp_test_env.prepend('LD_LIBRARY_PATH', _liblogger_libdir)
  endif
endif

test(
  'mcapp unit tests',
  mcapp_test_exe,
  env: mcapp_test_env,
  is_parallel: false,
  timeout: 300,
)
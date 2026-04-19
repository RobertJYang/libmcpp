mcapp_test_files = files(
  'test_base_app.cpp',
  'test_application.cpp',
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
    mcbase_test_utilities_dep,
  ],
  install: false,
)

mcapp_test_exe = executable(
  'mcapp_test',
  mcapp_test_main,
  link_whole: [
    mcapp_test_lib,
    mcapp_lib,
  ],
  include_directories: [incdir, srcdir, mcapp_test_incdir],
  dependencies: [
    gtest_dep,
    mcbase_test_utilities_dep,
    mcapp_dep,
    mcengine_dep,
  ],
  cpp_args: mcapp_test_cpp_args,
  link_args: mcapp_test_link_args,
  install: false,
)

mcapp_test_env = environment()
mcapp_test_env.set('MC_BUILD_ROOT', meson.project_build_root())

test(
  'mcapp unit tests',
  mcapp_test_exe,
  env: mcapp_test_env,
  timeout: 60,
)
